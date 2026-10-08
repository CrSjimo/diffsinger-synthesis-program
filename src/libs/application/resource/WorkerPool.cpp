#include "WorkerPool.h"

#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include <stdcorelib/support/logging.h>

namespace dssp::resource {
    namespace {
        stdc::LogCategory log("dssp.resource.WorkerPool");
    }
    struct WorkerPool::Impl {
        explicit Impl(std::string name) : name(std::move(name)) {}
        std::string name;
        std::mutex mutex;
        std::condition_variable_any wake;
        std::deque<std::function<void()>> pending;
        bool closed = false;
        std::once_flag shutdownOnce;
        // Destroy/join threads before any of the state they access.
        std::vector<std::jthread> workers;

        void run(std::stop_token stop) {
            for (;;) {
                std::function<void()> work;
                {
                    std::unique_lock lock(mutex);
                    // The stop-aware wait also makes partial construction safe.
                    wake.wait(lock, stop, [&] { return closed || !pending.empty(); });
                    if (pending.empty()) {
                        return;
                    }
                    work = std::move(pending.front());
                    pending.pop_front();
                }
                work();
            }
        }
    };

    std::expected<std::unique_ptr<WorkerPool>, WorkerError> WorkerPool::create(std::string name, std::size_t workers) {
        if (name.empty() || !workers) {
            return std::unexpected{WorkerError("Worker pool requires a name and at least one worker")};
        }
        try {
            auto pool = std::unique_ptr<WorkerPool>(new WorkerPool(std::make_unique<Impl>(std::move(name))));
            auto *impl = pool->impl_.get();
            impl->workers.reserve(workers);
            for (std::size_t i = 0; i < workers; ++i) {
                impl->workers.emplace_back([impl](std::stop_token stop) { impl->run(stop); });
            }
            return pool;
        } catch (const std::exception &error) {
            return std::unexpected{WorkerError("Cannot create worker threads", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"standard library", "worker thread construction", "exception"}))};
        }
    }
    WorkerPool::WorkerPool(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
    WorkerPool::~WorkerPool() { shutdown(); }

    std::expected<void, WorkerError> WorkerPool::dispatch(std::function<void()> work) {
        if (!work) {
            return std::unexpected{WorkerError("Cannot dispatch empty work")};
        }
        auto *impl = impl_.get();
        try {
            const std::lock_guard lock(impl->mutex);
            if (impl->closed) {
                return std::unexpected{WorkerError("Worker pool " + impl->name + " has been shut down")};
            }
            impl->pending.push_back(std::move(work));
        } catch (const std::exception &error) {
            return std::unexpected{WorkerError("Cannot enqueue background work", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"standard library", "worker queue insertion", "exception"}))};
        }
        impl->wake.notify_one();
        return {};
    }
    void WorkerPool::shutdown() {
        auto *impl = impl_.get();
        std::call_once(impl->shutdownOnce, [impl] {
            {
                const std::lock_guard lock(impl->mutex);
                impl->closed = true;
            }
            // The predicate transition is protected by the same mutex as wait.
            impl->wake.notify_all();
            for (auto &worker : impl->workers) {
                worker.join();
            }
            log.stdcTrace("Worker pool %1 stopped", impl->name);
        });
    }
}
