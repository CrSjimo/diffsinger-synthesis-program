#ifndef DSSP_INFERENCE_INFERENCEERROR_H
#define DSSP_INFERENCE_INFERENCEERROR_H

#include <dssperror/ErrorSet.h>

namespace dssp::inference {

    class InitializationError final : public ErrorType<InitializationError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.inference.initialization"; }
    };

    class ResourceError final : public ErrorType<ResourceError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.inference.resource"; }
    };

    class ExecutionError final : public ErrorType<ExecutionError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.inference.execution"; }
    };

    class CancelledError final : public ErrorType<CancelledError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.inference.cancelled"; }
    };
    using ExecutionErrors = ErrorSet<ExecutionError, CancelledError>;
    template <class T>
    using ExecutionResult = std::expected<T, ExecutionErrors>;

}

#endif // DSSP_INFERENCE_INFERENCEERROR_H
