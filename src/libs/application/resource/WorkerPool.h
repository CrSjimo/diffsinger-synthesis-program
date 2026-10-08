#ifndef DSSP_RESOURCE_WORKERPOOL_H
#define DSSP_RESOURCE_WORKERPOOL_H

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <dssperror/Error.h>

namespace dssp::resource {
    class WorkerError final : public ErrorType<WorkerError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.resource.worker"; }
    };

    // Shared worker threads only: resource queues own cancellation and ordering.
    // Work must not throw, and the owner retains the pool until clients finish.
    class WorkerPool final {
    public:
        static std::expected<std::unique_ptr<WorkerPool>, WorkerError> create(std::string name, std::size_t workers);
        ~WorkerPool();
        WorkerPool(const WorkerPool &) = delete;
        WorkerPool &operator=(const WorkerPool &) = delete;
        std::expected<void, WorkerError> dispatch(std::function<void()>);
        // Reject new work, drain accepted work, and join all workers. Never call
        // from a worker callback; resource owners cancel their queues beforehand.
        void shutdown();

    private:
        struct Impl;
        explicit WorkerPool(std::unique_ptr<Impl>);
        std::unique_ptr<Impl> impl_;
    };
}

#endif // DSSP_RESOURCE_WORKERPOOL_H
