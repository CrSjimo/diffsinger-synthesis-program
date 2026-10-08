#ifndef DSSP_RESOURCE_TASKEXECUTOR_H
#define DSSP_RESOURCE_TASKEXECUTOR_H

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>

#include <dssperror/ErrorSet.h>
#include <dsspresource/Async.h>

namespace dssp::resource {
    class TaskExecutionError final : public ErrorType<TaskExecutionError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.resource.task_execution"; }
    };
    class TaskCancelledError final : public ErrorType<TaskCancelledError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.resource.task_cancelled"; }
    };
    using TaskResult = std::expected<void, error::ErrorSet<TaskExecutionError, TaskCancelledError>>;

    // Ordinary background work, with no resource identity or cache. Callbacks
    // must not throw. The owner retains the executor until all clients finish.
    class TaskExecutor final {
    public:
        struct Task {
            std::function<void(std::stop_token)> run;
            // Exactly once, after run returns or immediately on queued cancellation.
            std::function<void(TaskResult)> complete;
        };
        static std::expected<std::shared_ptr<TaskExecutor>, TaskExecutionError> create(std::string name, std::size_t workers = 2);
        ~TaskExecutor();
        TaskExecutor(const TaskExecutor &) = delete;
        TaskExecutor &operator=(const TaskExecutor &) = delete;
        void submit(Task, std::stop_token = {});
        // Resume a coroutine on an ordinary worker. A failed/cancelled
        // admission is returned before the caller performs background work.
        auto resume(std::stop_token stop = {}) {
            return awaitCallback<TaskResult>([this, stop](auto complete) {
                submit({[](std::stop_token) {}, std::move(complete)}, stop);
            });
        }
        // Cancels pending work, requests running work to stop, and joins workers.
        // Never call from one of this executor's callbacks.
        void shutdown();

    private:
        struct Impl;
        explicit TaskExecutor(std::unique_ptr<Impl>);
        std::unique_ptr<Impl> impl_;
    };
}

#endif // DSSP_RESOURCE_TASKEXECUTOR_H
