#include "TaskExecutor.h"

#include <algorithm>
#include <exception>
#include <mutex>
#include <optional>
#include <vector>

#include <stdcorelib/support/logging.h>

#include <dsspresource/SerialQueue.h>
#include <dsspresource/WorkerPool.h>

namespace dssp::resource {
    namespace {
        stdc::LogCategory log("dssp.resource.TaskExecutor");

        struct Submission {
            std::mutex mutex;
            std::optional<TaskExecutionError> dispatchError;
        };
    }

    struct TaskExecutor::Impl {
        Impl(std::unique_ptr<WorkerPool> executor, std::string name) : executor(std::move(executor)), name(std::move(name)) {}

        std::unique_ptr<WorkerPool> executor;
        std::string name;
        std::mutex mutex;
        std::vector<std::weak_ptr<SerialQueue>> jobs;
        bool stopped = false;
        std::once_flag shutdownOnce;
    };

    std::expected<std::shared_ptr<TaskExecutor>, TaskExecutionError> TaskExecutor::create(std::string name, std::size_t workers) {
        if (name.empty() || !workers) {
            return std::unexpected{TaskExecutionError("Task executor requires a name and at least one worker")};
        }

        auto executor = WorkerPool::create(name, workers);
        if (!executor) {
            return std::unexpected{TaskExecutionError("Cannot create background task workers", executor.error().clone())};
        }

        try {
            return std::shared_ptr<TaskExecutor>(new TaskExecutor(std::make_unique<Impl>(std::move(*executor), std::move(name))));
        } catch (const std::exception &error) {
            return std::unexpected{TaskExecutionError("Cannot create background task workers", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"standard library", "TaskExecutor construction", "exception"}))};
        }
    }

    TaskExecutor::TaskExecutor(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

    TaskExecutor::~TaskExecutor() { shutdown(); }

    void TaskExecutor::submit(Task task, std::stop_token stop) {
        auto *impl = impl_.get();
        const auto submission = std::make_shared<Submission>();

        // A one-item SerialQueue supplies the existing cancellation/lifecycle
        // rules. The worker pool schedules unrelated jobs concurrently.
        const auto queueReference = std::make_shared<std::weak_ptr<SerialQueue>>();
        const auto queue = std::make_shared<SerialQueue>([impl, submission, queueReference](auto work) {
            const auto queue = queueReference->lock();
            // Keep the queue alive until its drain has cleared running and
            // scheduled state, rather than closing it inside completion.
            auto dispatched = impl->executor->dispatch([queue, work] { work(); });
            if (dispatched) {
                return;
            }

            {
                const std::lock_guard lock(submission->mutex);
                submission->dispatchError.emplace("Cannot dispatch background task", dispatched.error().clone());
            }
            if (queue) {
                queue->close();
            }

            // Clear SerialQueue's scheduled drain after closing; no user work runs.
            work();
        }, impl->name);
        *queueReference = queue;

        bool stopped;
        {
            const std::lock_guard lock(impl->mutex);
            stopped = impl->stopped;
            if (!stopped) {
                std::erase_if(impl->jobs, [](const auto &job) { return job.expired(); });
                impl->jobs.push_back(queue);
            }
        }

        if (stopped) {
            queue->close();
        }

        queue->submit({std::move(task.run), [queue, submission, complete = std::move(task.complete)](bool cancelled) {
            TaskResult result;
            {
                const std::lock_guard lock(submission->mutex);
                if (submission->dispatchError) {
                    result = std::unexpected{error::ErrorSet<TaskExecutionError, TaskCancelledError>{*submission->dispatchError}};
                } else if (cancelled) {
                    result = std::unexpected{error::ErrorSet<TaskExecutionError, TaskCancelledError>{TaskCancelledError("Background task was cancelled")}};
                }
            }

            complete(std::move(result));
        }}, stop);
    }

    void TaskExecutor::shutdown() {
        auto *impl = impl_.get();
        std::call_once(impl->shutdownOnce, [impl] {
            std::vector<std::shared_ptr<SerialQueue>> jobs;
            {
                const std::lock_guard lock(impl->mutex);
                impl->stopped = true;
                for (const auto &job : impl->jobs) {
                    if (auto queue = job.lock()) {
                        jobs.push_back(std::move(queue));
                    }
                }
            }

            for (const auto &job : jobs) {
                job->close();
            }

            for (const auto &job : jobs) {
                job->wait();
            }

            impl->executor->shutdown();
            log.stdcTrace("Background executor %1 stopped", impl->name);
        });
    }
}
