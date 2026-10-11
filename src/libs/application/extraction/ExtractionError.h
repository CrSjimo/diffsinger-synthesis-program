#ifndef DSSP_EXTRACTION_EXTRACTIONERROR_H
#define DSSP_EXTRACTION_EXTRACTIONERROR_H

#include <dssperror/ErrorSet.h>

namespace dssp::extraction {
    class ResourceNotFoundError final : public ErrorType<ResourceNotFoundError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.extraction.not_found"; }
    };

    class InvalidInputError final : public ErrorType<InvalidInputError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.extraction.invalid_input"; }
    };

    class OperationError final : public ErrorType<OperationError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.extraction.operation"; }
    };

    class CancelledError final : public ErrorType<CancelledError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.extraction.cancelled"; }
    };

    using CatalogErrors = ErrorSet<ResourceNotFoundError, OperationError>;
    using ExtractionErrors = ErrorSet<InvalidInputError, OperationError, CancelledError>;
    template <class T> using CatalogResult = std::expected<T, CatalogErrors>;
    template <class T> using ExtractionResult = std::expected<T, ExtractionErrors>;
}

#endif // DSSP_EXTRACTION_EXTRACTIONERROR_H
