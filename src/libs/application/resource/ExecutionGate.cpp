#include "ExecutionGate.h"

#include <atomic>
#include <mutex>

#include <dsspresource/SerialQueue.h>
#include <dsspresource/WorkerPool.h>

namespace dssp::resource {
    class ExecutionGate::Impl {
    public:
        std::unique_ptr<WorkerPool> worker;
        std::shared_ptr<SerialQueue> queue;
        std::once_flag shutdownOnce;
    };

    std::expected<std::shared_ptr<ExecutionGate>, WorkerError> ExecutionGate::create(std::string name) {
        auto worker = WorkerPool::create(name, 1);
        if (!worker) return std::unexpected{std::move(worker.error())};
        auto impl = std::make_unique<Impl>();
        impl->worker = std::move(*worker);
        impl->queue = std::make_shared<SerialQueue>([impl = impl.get()](auto work) {
            auto dispatched = impl->worker->dispatch(work);
            if (!dispatched) {
                impl->queue->close();
                work();
            }
        }, std::move(name));
        return std::shared_ptr<ExecutionGate>(new ExecutionGate(std::move(impl)));
    }
    ExecutionGate::ExecutionGate(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
    ExecutionGate::~ExecutionGate() { shutdown(); }
    drogon::Task<PermitResult> ExecutionGate::acquire(std::stop_token stop) {
        co_return co_await awaitCallback<PermitResult>([queue = impl_->queue, stop](auto complete) {
            const auto granted = std::make_shared<std::atomic<bool>>(false);
            queue->submit({{}, [granted, complete](bool) mutable {
                if (!granted->load(std::memory_order_acquire)) complete(std::unexpected{TaskCancelledError("Execution admission was cancelled")});
            }, [granted, complete](std::stop_token token, auto finish) mutable {
                granted->store(true, std::memory_order_release);
                complete(ExecutionPermit(std::move(finish), token));
            }}, stop);
        });
    }
    void ExecutionGate::close() { impl_->queue->close(); }
    void ExecutionGate::shutdown() {
        std::call_once(impl_->shutdownOnce, [&] {
            close();
            impl_->queue->wait();
            impl_->worker->shutdown();
        });
    }
}
