#include "InferencePool.h"

#include <condition_variable>
#include <exception>
#include <map>
#include <mutex>
#include <thread>

#include <stdcorelib/support/logging.h>
#include <synthrt/Core/ContribImportBinding.h>
#include <trantor/utils/ConcurrentTaskQueue.h>

#include <dsspinference/ResourceIdentity.h>
#include <dsspresource/LruCache.h>
#include <dsspresource/SerialQueue.h>

namespace dssp::inference {
    namespace {
        stdc::LogCategory log("dssp.inference.InferencePool");

        struct LoadedInference {
            std::string label;
            std::unique_ptr<srt::InferenceExecutive> executive;
            ~LoadedInference() {
                if (!executive) {
                    return;
                }
                log.stdcDebug("Releasing inference resource %1", label);
                // start() is synchronous, and the lease outlives its stop callback.
                // The wait also covers native implementations with worker cleanup.
                auto waited = nativeCall("InferenceExecutive::waitForFinished", [&] { return executive->waitForFinished(); });
                if (!waited) {
                    log.stdcWarning("Cannot finish releasing inference %1: %2", label, waited.error().formatChain());
                }
            }
        };
    }

    class InferenceResource {
    public:
        InferenceDefinition definition;
        std::shared_ptr<resource::SerialQueue> queue;
        const void *owner = nullptr;
    };

    class InferencePool::Impl {
    public:
        Impl(std::string group, resource::CacheOptions options) : group(std::move(group)), options(options), cache(options), executor(4, "dssp-inference") {}
        std::string group;
        resource::CacheOptions options;
        resource::LruCache<std::string, std::unique_ptr<LoadedInference>> cache;
        trantor::ConcurrentTaskQueue executor;
        std::mutex mutex;
        std::map<std::string, InferenceHandle> resources;
        bool stopped = false;
        std::once_flag shutdownOnce;
        std::mutex cleanupMutex;
        std::condition_variable_any cleanupWake;
        std::jthread cleanup;
    };

    std::expected<std::unique_ptr<InferencePool>, InitializationError> InferencePool::create(std::string group, resource::CacheOptions options) {
        const auto maximumInterval = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::duration::max() / 2);
        if (group.empty() || options.idleTimeout.count() < 0 || options.cleanupInterval.count() <= 0 || options.cleanupInterval > maximumInterval) {
            return std::unexpected{InitializationError("Invalid inference cache group options")};
        }
        try {
            auto pool = std::unique_ptr<InferencePool>(new InferencePool(std::make_unique<Impl>(std::move(group), options)));
            auto *impl = pool->impl_.get();
            if (options.idleTimeout.count() > 0) {
                impl->cleanup = std::jthread([impl](std::stop_token stop) {
                    std::unique_lock lock(impl->cleanupMutex);
                    while (!stop.stop_requested()) {
                        impl->cleanupWake.wait_for(lock, stop, impl->options.cleanupInterval, [] { return false; });
                        if (!stop.stop_requested()) {
                            lock.unlock();
                            impl->cache.sweep();
                            lock.lock();
                        }
                    }
                });
            }
            log.stdcDebug("Inference cache group %1: capacity=%2 idle_timeout_ms=%3 cleanup_interval_ms=%4", impl->group, options.capacity, options.idleTimeout.count(), options.cleanupInterval.count());
            return pool;
        } catch (const std::exception &error) {
            return std::unexpected{InitializationError("Cannot create inference workers", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"Trantor/standard threading", "create worker threads", "exception"}))};
        }
    }
    InferencePool::InferencePool(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
    InferencePool::~InferencePool() { shutdown(); }

    std::expected<InferenceDefinition, ExecutionError> InferencePool::importDefinition(const srt::ContribImport &imported, std::shared_ptr<const srt::InferenceRuntimeOptions> options, const srt::JsonValue &effectiveRuntime) {
        try {
            const auto *binding = imported.binding();
            if (!binding || binding->state() != srt::ContribImportBinding::Active || !options) {
                return std::unexpected{ExecutionError("Inference import is not active")};
            }
            auto &target = binding->target();
            auto *spec = dynamic_cast<srt::InferenceSpec *>(&target);
            if (!spec || options->interface() != target.interface() || options->variant() != target.variant() || options->level() != target.level()) {
                return std::unexpected{ExecutionError("Inference runtime options do not match the resolved import")};
            }
            const auto *importOptions = &binding->options();
            const auto key = fingerprint(srt::JsonValue(srt::JsonObject{{"resource", resourceId(target)}, {"import", imported.manifestOptions()}, {"runtime", effectiveRuntime}}).toJson());
            if (!key) {
                return std::unexpected{ExecutionError("Cannot identify inference resource", key.error().clone())};
            }
            InferenceDefinition definition;
            definition.key = *key;
            definition.label = resourceReference(target);
            definition.packages = {binding->importer().package(), target.package()};
            definition.create = [spec, importOptions, options = std::move(options)] {
                return nativeCall("InferenceSpec::createInference", [&] { return spec->createInference(*importOptions, *options); });
            };
            return definition;
        } catch (const std::exception &error) {
            return std::unexpected{ExecutionError("Cannot inspect inference import", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"synthrt", "inspect inference import", "exception"}))};
        }
    }
    std::expected<InferenceHandle, ExecutionError> InferencePool::bind(InferenceDefinition definition) {
        if (definition.key.empty() || !definition.create) {
            return std::unexpected{ExecutionError("Inference resource requires an identity and a factory")};
        }
        const std::lock_guard lock(impl_->mutex);
        if (impl_->stopped) {
            return std::unexpected{ExecutionError("Inference cache group has been shut down")};
        }
        if (const auto found = impl_->resources.find(definition.key); found != impl_->resources.end()) {
            return found->second;
        }
        auto resource = std::make_shared<InferenceResource>();
        resource->definition = std::move(definition);
        resource->owner = impl_.get();
        resource->queue = std::make_shared<resource::SerialQueue>([impl = impl_.get()](std::function<void()> task) { impl->executor.runTaskInQueue(std::move(task)); }, impl_->group + "/" + resource->definition.label + "#" + resource->definition.key.substr(0, 12));
        impl_->resources.emplace(resource->definition.key, resource);
        return InferenceHandle(std::move(resource));
    }
    void InferencePool::schedule(InferenceHandle handle, std::stop_token stop, std::function<bool(srt::InferenceExecutive &)> run, std::function<void(std::optional<ExecutionError>, bool)> complete) {
        if (!handle || handle->owner != impl_.get()) {
            complete(ExecutionError("Inference resource belongs to another cache group"), stop.stop_requested());
            return;
        }
        const auto error = std::make_shared<std::optional<ExecutionError>>();
        handle->queue->submit({[impl = impl_.get(), handle, error, run = std::move(run)](std::stop_token token) {
            auto lease = impl->cache.acquire(handle->definition.key);
            if (!lease) {
                auto created = handle->definition.create();
                if (!created) {
                    error->emplace(std::move(created.error()));
                    return;
                }
                if (!*created) {
                    error->emplace("Inference provider returned a null executive");
                    return;
                }
                auto loaded = std::make_unique<LoadedInference>();
                loaded->label = handle->definition.label;
                loaded->executive = std::move(*created);
                log.stdcDebug("Loaded inference resource %1 into cache group %2", loaded->label, impl->group);
                lease.emplace(impl->cache.insert(handle->definition.key, std::move(loaded)));
            }
            auto &executive = *lease->value()->executive;
            bool succeeded = false;
            {
                const std::stop_callback cancellation(token, [&] {
                    log.stdcTrace("Requesting stop for inference %1", handle->definition.label);
                    auto stopped = nativeCall("InferenceExecutive::stop", [&] { return executive.stop(); });
                    if (!stopped) {
                        log.stdcWarning("Cannot request stop for inference %1: %2", handle->definition.label, stopped.error().formatChain());
                    }
                });
                if (!token.stop_requested()) {
                    succeeded = run(executive);
                }
            }
            if (!succeeded || token.stop_requested() || executive.state() == srt::ITask::Canceled || executive.state() == srt::ITask::Failed) {
                // Cancelled/failed native tasks may retain stop or failure state.
                // Recreate on the next task rather than infecting another request.
                impl->cache.erase(handle->definition.key);
            }
        }, [error, complete = std::move(complete)](bool cancelled) mutable { complete(std::move(*error), cancelled); }}, stop);
    }
    std::size_t InferencePool::residentCount() const { return impl_->cache.size(); }
    void InferencePool::sweep() { impl_->cache.sweep(); }
    void InferencePool::shutdown() {
        std::call_once(impl_->shutdownOnce, [&] {
            std::vector<InferenceHandle> resources;
            {
                const std::lock_guard lock(impl_->mutex);
                impl_->stopped = true;
                for (const auto &[key, resource] : impl_->resources) {
                    resources.push_back(resource);
                }
            }
            impl_->cleanup.request_stop();
            if (impl_->cleanup.joinable()) {
                impl_->cleanup.join();
            }
            for (const auto &resource : resources) {
                resource->queue->close();
            }
            for (const auto &resource : resources) {
                resource->queue->wait();
            }
            impl_->executor.stop();
            impl_->cache.clear();
            // Keep closed definitions valid for outstanding non-running handles.
            log.stdcDebug("Inference cache group %1 stopped", impl_->group);
        });
    }
}
