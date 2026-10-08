#include "SerialQueue.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <iterator>
#include <list>
#include <mutex>
#include <utility>

#include <stdcorelib/support/logging.h>

namespace dssp::resource {
    namespace {
        stdc::LogCategory log("dssp.resource.SerialQueue");
    }

    struct SerialQueue::State : std::enable_shared_from_this<SerialQueue::State> {
        enum class Phase {
            Preparing,
            Queued,
            Running,
            Done
        };
        struct Item {
            Task task;
            std::stop_source stop;
            std::unique_ptr<std::stop_callback<std::function<void()>>> cancellation;
            Phase phase = Phase::Preparing;
            std::uint64_t sequence = 0;
            std::list<std::shared_ptr<Item>>::iterator position{};
        };
        State(Dispatch dispatch, std::string name) : dispatch(std::move(dispatch)), name(std::move(name)) {}
        Dispatch dispatch;
        std::string name;
        std::mutex mutex;
        std::condition_variable idle;
        std::list<std::shared_ptr<Item>> pending;
        std::shared_ptr<Item> running;
        std::size_t completing = 0;
        std::uint64_t sequence = 0;
        bool scheduled = false;
        bool closed = false;

        void finishCancelled(const std::shared_ptr<Item> &item) {
            item->task.complete(true);
            item->task = {};
            const std::lock_guard lock(mutex);
            --completing;
            idle.notify_all();
        }
        void cancel(const std::shared_ptr<Item> &item) {
            item->stop.request_stop();
            bool complete = false;
            {
                const std::lock_guard lock(mutex);
                if (item->phase == Phase::Queued) {
                    pending.erase(item->position);
                    item->phase = Phase::Done;
                    ++completing;
                    complete = true;
                }
            }
            if (complete) {
                log.stdcTrace("Queue %1 cancelled waiting task %2", name, item->sequence);
                finishCancelled(item);
            }
        }
        void drain() {
            for (;;) {
                std::shared_ptr<Item> item;
                {
                    const std::lock_guard lock(mutex);
                    if (pending.empty()) {
                        scheduled = false;
                        idle.notify_all();
                        return;
                    }
                    item = std::move(pending.front());
                    pending.pop_front();
                    item->phase = Phase::Running;
                    running = item;
                }
                log.stdcTrace("Queue %1 starting task %2", name, item->sequence);
                if (!item->stop.stop_requested() && item->task.runAsync) {
                    auto start = item->task.runAsync;
                    const auto finished = std::make_shared<std::atomic_flag>();
                    start(item->stop.get_token(), [state = shared_from_this(), item, finished] {
                        if (finished->test_and_set(std::memory_order_acq_rel)) return;
                        state->finish(item);
                        // Suspension leaves scheduled set, so no second drain
                        // can start this resource while its task is outstanding.
                        state->dispatch([state] { state->drain(); });
                    });
                    return;
                }
                if (!item->stop.stop_requested()) {
                    item->task.run(item->stop.get_token());
                }
                finish(item);
            }
        }
        void finish(const std::shared_ptr<Item> &item) {
            // Do not let a late request cancellation reach the next task.
            item->cancellation.reset();
            item->task.complete(item->stop.stop_requested());
            item->task = {};
            {
                const std::lock_guard lock(mutex);
                item->phase = Phase::Done;
                running.reset();
                idle.notify_all();
            }
            log.stdcTrace("Queue %1 finished task %2 (cancelled=%3)", name, item->sequence, item->stop.stop_requested());
        }
    };

    SerialQueue::SerialQueue(Dispatch dispatch, std::string name) : state_(std::make_shared<State>(std::move(dispatch), std::move(name))) {}
    SerialQueue::~SerialQueue() { close(); }

    void SerialQueue::submit(Task task, std::stop_token stop) {
        const auto state = state_;
        auto item = std::make_shared<State::Item>();
        item->task = std::move(task);
        const std::weak_ptr<State> weakState = state;
        const std::weak_ptr<State::Item> weakItem = item;
        item->cancellation = std::make_unique<std::stop_callback<std::function<void()>>>(stop, [weakState, weakItem] {
            const auto state = weakState.lock();
            const auto item = weakItem.lock();
            if (state && item) {
                state->cancel(item);
            }
        });
        bool dispatch = false;
        bool cancelled = false;
        {
            const std::lock_guard lock(state->mutex);
            item->sequence = ++state->sequence;
            cancelled = state->closed || item->stop.stop_requested();
            if (cancelled) {
                item->phase = State::Phase::Done;
                ++state->completing;
            } else {
                item->phase = State::Phase::Queued;
                state->pending.push_back(item);
                item->position = std::prev(state->pending.end());
                dispatch = !std::exchange(state->scheduled, true);
            }
        }
        if (cancelled) {
            state->finishCancelled(item);
        } else if (dispatch) {
            state->dispatch([state] { state->drain(); });
        }
    }
    void SerialQueue::close() {
        const auto state = state_;
        std::list<std::shared_ptr<State::Item>> pending;
        std::shared_ptr<State::Item> running;
        {
            const std::lock_guard lock(state->mutex);
            state->closed = true;
            pending.swap(state->pending);
            state->completing += pending.size();
            for (const auto &item : pending) {
                item->phase = State::Phase::Done;
            }
            running = state->running;
        }
        for (const auto &item : pending) {
            item->stop.request_stop();
            state->finishCancelled(item);
        }
        if (running) {
            running->stop.request_stop();
        }
    }
    void SerialQueue::wait() {
        const auto state = state_;
        std::unique_lock lock(state->mutex);
        state->idle.wait(lock, [&] { return !state->scheduled && !state->running && !state->completing; });
    }
}
