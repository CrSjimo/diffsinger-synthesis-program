#ifndef DSSP_RESOURCE_ASYNC_H
#define DSSP_RESOURCE_ASYNC_H

#include <atomic>
#include <coroutine>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <drogon/utils/coroutine.h>

namespace dssp::resource {

    // A thin bridge for APIs whose completion may run inline or on another
    // thread. Task ownership must extend until completion, including cancellation.
    // The producer decides where to resume; this adapter does not create threads.
    template <class Result, class Start>
    class CallbackAwaiter final {
    public:
        explicit CallbackAwaiter(Start start) : start_(std::move(start)), state_(std::make_shared<State>()) {}

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> continuation) {
            // Publish only shared state. An inline callback can destroy this
            // awaiter before start() returns, so nothing accesses it afterwards.
            auto start = std::move(start_);
            start([state = state_, continuation](Result result) mutable {
                if (state->completed.test_and_set(std::memory_order_acq_rel)) return;
                state->result.emplace(std::move(result));
                continuation.resume();
            });
        }

        Result await_resume() { return std::move(*state_->result); }

    private:
        struct State {
            std::atomic_flag completed = ATOMIC_FLAG_INIT;
            std::optional<Result> result;
        };

        Start start_;
        std::shared_ptr<State> state_;
    };

    template <class Result, class Start>
    auto awaitCallback(Start start) { return CallbackAwaiter<Result, Start>(std::move(start)); }
}

#endif // DSSP_RESOURCE_ASYNC_H
