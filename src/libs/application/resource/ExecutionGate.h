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
        ExecutionPermit(std::function<void()> release, std::stop_token stop) : m_release(std::move(release)), m_stop(stop) {}

        ExecutionPermit(ExecutionPermit &&other) noexcept : m_release(std::exchange(other.m_release, {})), m_stop(other.m_stop) {}

        ExecutionPermit &operator=(ExecutionPermit &&other) noexcept {
            if (this != &other) {
                release();
                m_release = std::exchange(other.m_release, {});
                m_stop = other.m_stop;
            }
            return *this;
        }

        ~ExecutionPermit() { release(); }
        std::stop_token cancellation() const noexcept { return m_stop; }

    private:
        void release() { if (auto release = std::exchange(m_release, {})) release(); }
        std::function<void()> m_release;
        std::stop_token m_stop;
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
        std::unique_ptr<Impl> m_impl;
    };
}

#endif // DSSP_RESOURCE_EXECUTIONGATE_H
