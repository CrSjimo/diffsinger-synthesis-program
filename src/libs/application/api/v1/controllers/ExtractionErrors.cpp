#include "ExtractionErrors.h"

#include <stdcorelib/support/logging.h>

namespace dssp::api_v1 {
    namespace {
        stdc::LogCategory log("dssp.api_v1.ExtractionErrors");
        template <class Errors>
        drogon::HttpResponsePtr present(const Errors &errors, const http::RequestLog &context) {
            return errors.visit(Overload{
                [&context](const extraction::ResourceNotFoundError &error) {
                    return http::problemResponse(404, "resource-not-found", "Resource not found", error.message(), context);
                },
                [&context](const extraction::InvalidInputError &error) {
                    context.problem("invalid-input", error.message());
                    return http::problemResponse(422, "invalid-input", "Invalid input", error.message(), context);
                },
                [&context](const extraction::OperationError &error) {
                    log.stdcCritical("%1 Extraction failed: %2", context.description(), http::quoteLogValue(error.formatChain(), 8192));
                    return http::problemResponse(500, "extraction", "Internal server error", error.message(), context);
                },
                [&context](const extraction::CancelledError &error) {
                    log.stdcCritical("%1 Extraction cancelled while the HTTP request was still active: %2", context.description(), http::quoteLogValue(error.formatChain(), 8192));
                    return http::problemResponse(500, "cancelled", "Operation cancelled", error.message(), context);
                }});
        }
    }
    drogon::HttpResponsePtr extractionProblemResponse(const extraction::CatalogErrors &errors, const http::RequestLog &context) { return present(errors, context); }
    drogon::HttpResponsePtr extractionProblemResponse(const extraction::ExtractionErrors &errors, const http::RequestLog &context) { return present(errors, context); }
}
