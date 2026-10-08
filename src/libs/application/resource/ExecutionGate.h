#ifndef DSSP_RESOURCE_EXECUTIONGATE_H
#define DSSP_RESOURCE_EXECUTIONGATE_H

#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

#include <dsspresource/Async.h>
#include <dsspresource/TaskExecutor.h>
#include <dsspresource/WorkerPool.h>

namespace dssp::resource {
    // A move-only permit retains one FIFO position until its owner has finished
    // using the protected resource. Cancellation alone never releases it.
    class ExecutionPermit final {
    public:
        ExecutionPermit(std::function<void()> release, std::stop_token stop) : release_(std::move(release)), stop_(stop) {}
        ExecutionPermit(ExecutionPermit &&other) noexcept : release_(std::exchange(other.release_, {})), stop_(other.stop_) {}
        ExecutionPermit &operator=(ExecutionPermit &&other) noexcept {
            if (this != &other) {
                release();
                release_ = std::exchange(other.release_, {});
                stop_ = other.stop_;
            }
            return *this;
        }
        ~ExecutionPermit() { release(); }
        std::stop_token cancellation() const noexcept { return stop_; }

    private:
        void release() { if (auto release = std::exchange(release_, {})) release(); }
        std::function<void()> release_;
        std::stop_token stop_;
    };
    using PermitResult = std::expected<ExecutionPermit, TaskCancelledError>;

    // Resource-agnostic FIFO admission. Waiters suspend instead of occupying
    // workers. Admission resumes on this gate's worker, never under queue locks.
    class ExecutionGate final {
    public:
        static std::expected<std::shared_ptr<ExecutionGate>, WorkerError> create(std::string name);
        ~ExecutionGate();
        ExecutionGate(const ExecutionGate &) = delete;
        ExecutionGate &operator=(const ExecutionGate &) = delete;
        drogon::Task<PermitResult> acquire(std::stop_token = {});
        void close();
        void shutdown();

    private:
        class Impl;
        explicit ExecutionGate(std::unique_ptr<Impl>);
        std::unique_ptr<Impl> impl_;
    };
}

#endif // DSSP_RESOURCE_EXECUTIONGATE_H
