#ifndef DSSP_ARCH_ERRORS_H
#define DSSP_ARCH_ERRORS_H

#include <expected>
#include <string_view>

#include <dssperror/ErrorSet.h>

namespace dssp::arch {

    class ResourceNotFoundError final : public ErrorType<ResourceNotFoundError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.arch.not-found"; }
    };

    class InvalidInputError final : public ErrorType<InvalidInputError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.arch.invalid-input"; }
    };

    // Reserve this type for identified internal/resource/execution faults.
    // Missing capabilities and invalid inputs use InvalidInputError instead.
    class OperationError final : public ErrorType<OperationError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.arch.operation"; }
    };

    class CancelledError final : public ErrorType<CancelledError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.arch.cancelled"; }
    };

    using CatalogErrors = ErrorSet<ResourceNotFoundError, InvalidInputError, OperationError>;
    using EnvironmentErrors = ErrorSet<InvalidInputError, OperationError>;
    using SynthesisErrors = ErrorSet<InvalidInputError, OperationError, CancelledError>;
    template <class T>
    using CatalogResult = std::expected<T, CatalogErrors>;
    template <class T>
    using SynthesisResult = std::expected<T, SynthesisErrors>;

}

#endif // DSSP_ARCH_ERRORS_H
