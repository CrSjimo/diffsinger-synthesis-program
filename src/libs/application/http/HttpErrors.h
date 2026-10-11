#ifndef DSSP_HTTP_HTTPERRORS_H
#define DSSP_HTTP_HTTPERRORS_H

#include <vector>

#include <dssperror/ErrorSet.h>

namespace dssp::http {

    struct FieldIssue {
        std::string pointer;
        std::string errorType;
        std::string detail;
    };

    class MalformedJsonError final : public ErrorType<MalformedJsonError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.http.malformed-json"; }
    };

    class UnsupportedPayloadError final : public ErrorType<UnsupportedPayloadError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.http.unsupported-payload"; }
    };

    class ValidationError final : public ErrorType<ValidationError> {
    public:
        explicit ValidationError(std::vector<FieldIssue> issues, std::source_location location = std::source_location::current()) : ErrorType("Request fields do not conform to the API schema", {}, location), issues_(std::make_shared<const std::vector<FieldIssue>>(std::move(issues))) {}
        const std::vector<FieldIssue> &issues() const noexcept { return *issues_; }
        std::string_view kind() const noexcept override { return "dssp.http.validation"; }

    private:
        std::shared_ptr<const std::vector<FieldIssue>> issues_;
    };

    class HttpBoundaryError final : public ErrorType<HttpBoundaryError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.http.boundary"; }
    };

    using PayloadErrors = ErrorSet<MalformedJsonError, UnsupportedPayloadError, HttpBoundaryError>;
    using DtoErrors = ErrorSet<ValidationError, HttpBoundaryError>;
    using RequestErrors = ErrorSet<MalformedJsonError, UnsupportedPayloadError, ValidationError, HttpBoundaryError>;

}

#endif // DSSP_HTTP_HTTPERRORS_H
