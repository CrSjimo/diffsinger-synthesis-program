#ifndef DSSP_INFERENCE_INFERENCEPOOL_H
#define DSSP_INFERENCE_INFERENCEPOOL_H

#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Windows SDK headers define interface as a macro; synthrt uses it as an identifier.
#ifdef _WIN32
#    pragma push_macro("interface")
#    undef interface
#endif

#include <synthrt/Core/PackageHandle.h>
#include <synthrt/SVS/InferenceContrib.h>

#ifdef _WIN32
#    pragma pop_macro("interface")
#endif

#include <dsspinference/InferenceError.h>
#include <dsspinference/NativeCall.h>
#include <dsspresource/CacheOptions.h>

namespace dssp::inference {

    class InferenceResource;
    using InferenceHandle = std::shared_ptr<const InferenceResource>;

    struct InferenceDefinition {
        std::string key;
        std::string label;
        // Retain both the target and the importer owning typed import options.
        std::vector<srt::PackageHandle> packages;
        // The factory returns a ready-to-run executive without throwing.
        std::function<std::expected<std::unique_ptr<srt::InferenceExecutive>, ExecutionError>()> create;
    };

    // One independently configured cache group. Resource definitions and FIFO
    // queues survive eviction; only initialized executives occupy cache entries.
    class InferencePool final {
    public:
        static std::expected<std::unique_ptr<InferencePool>, InitializationError> create(std::string group, resource::CacheOptions = {});
        ~InferencePool();
        InferencePool(const InferencePool &) = delete;
        InferencePool &operator=(const InferencePool &) = delete;
        std::expected<InferenceHandle, ExecutionError> bind(InferenceDefinition);

        template <class Executive, class InitArgs>
        std::expected<InferenceHandle, ExecutionError> bindImport(const srt::ContribImport &imported, std::shared_ptr<const srt::InferenceRuntimeOptions> options, const srt::JsonValue &effectiveRuntime) {
            auto definition = importDefinition(imported, std::move(options), effectiveRuntime);
            if (!definition) {
                return std::unexpected{std::move(definition.error())};
            }
            auto factory = std::move(definition->create);
            definition->create = [factory = std::move(factory)]() -> std::expected<std::unique_ptr<srt::InferenceExecutive>, ExecutionError> {
                auto created = factory();
                if (!created) {
                    return std::unexpected{std::move(created.error())};
                }
                auto *typed = dynamic_cast<Executive *>(created->get());
                if (!typed) {
                    return std::unexpected{ExecutionError("Inference provider returned an incompatible executive")};
                }
                auto initialized = nativeCall("InferenceExecutive::initialize", [&] { return typed->initialize(InitArgs{}); });
                if (!initialized) {
                    return std::unexpected{std::move(initialized.error())};
                }
                return std::move(*created);
            };
            return bind(std::move(*definition));
        }

        // Invoke the synchronous native task contract on a pool worker. The
        // function must finish the task before returning its srt::Expected.
        template <class Executive, class Function, class Completion>
        void run(InferenceHandle handle, std::stop_token stop, Function function, Completion complete) {
            using Value = typename std::invoke_result_t<Function, Executive &>::value_type;
            const auto result = std::make_shared<std::optional<ExecutionResult<Value>>>();
            schedule(std::move(handle), stop, [result, function = std::move(function)](srt::InferenceExecutive &executive) mutable {
                auto *typed = dynamic_cast<Executive *>(&executive);
                if (!typed) {
                    result->emplace(fail<ExecutionErrors>(ExecutionError("Inference resource does not match the requested contract")));
                    return false;
                }
                auto native = nativeCall("InferenceExecutive::start", [&] { return std::invoke(function, *typed); });
                if (!native) {
                    result->emplace(fail<ExecutionErrors>(std::move(native.error())));
                    return false;
                } else if (executive.state() == srt::ITask::Canceled) {
                    result->emplace(fail<ExecutionErrors>(CancelledError("Inference execution was cancelled")));
                    return false;
                } else if (executive.state() == srt::ITask::Failed) {
                    result->emplace(fail<ExecutionErrors>(ExecutionError("Inference reported a failed task with a successful result")));
                    return false;
                } else if constexpr (std::is_void_v<Value>) {
                    result->emplace();
                } else {
                    result->emplace(std::move(*native));
                }
                return true;
            }, [result, complete = std::move(complete)](std::optional<ExecutionError> error, bool cancelled) mutable {
                if (cancelled) {
                    complete(ExecutionResult<Value>{fail<ExecutionErrors>(CancelledError("Inference request was cancelled"))});
                } else if (error) {
                    complete(ExecutionResult<Value>{fail<ExecutionErrors>(std::move(*error))});
                } else if (*result) {
                    complete(std::move(**result));
                } else {
                    complete(ExecutionResult<Value>{fail<ExecutionErrors>(ExecutionError("Inference task completed without a result"))});
                }
            });
        }

        std::size_t residentCount() const;
        void sweep();
        // Cancel queues, request native stop, wait for callbacks, release all
        // executives. Call on the host thread before releasing the SynthUnit.
        void shutdown();

    private:
        class Impl;
        explicit InferencePool(std::unique_ptr<Impl>);
        static std::expected<InferenceDefinition, ExecutionError> importDefinition(const srt::ContribImport &, std::shared_ptr<const srt::InferenceRuntimeOptions>, const srt::JsonValue &);
        void schedule(InferenceHandle, std::stop_token, std::function<bool(srt::InferenceExecutive &)>, std::function<void(std::optional<ExecutionError>, bool)>);
        std::unique_ptr<Impl> impl_;
    };
}

#endif // DSSP_INFERENCE_INFERENCEPOOL_H
