#include "ArchitectureErrors.h"

#include <stdcorelib/support/logging.h>

#include <dssphttp/Response.h>

namespace dssp::api_v1 {
    namespace {
        stdc::LogCategory log("dssp.api_v1.ArchitectureErrors");

        template <class Errors>
        drogon::HttpResponsePtr present(const Errors &errors) {
            return errors.visit(Overload{
                [](const arch::ResourceNotFoundError &error) { return http::problemResponse(404, "resource-not-found", "Resource not found", error.message()); },
                [](const arch::InvalidInputError &error) { return http::problemResponse(422, "invalid-input", "Invalid input", error.message()); },
                [](const arch::OperationError &error) {
                    log.stdcCritical("Architecture operation failed: %1", error.formatChain());
                    return http::problemResponse(500, "architecture", "Internal server error", "Architecture operation failed");
                },
                [](const arch::CancelledError &) {
                    // A stopped request suppresses sending. Otherwise complete
                    // the HTTP request even if the backend reported cancellation.
                    return http::problemResponse(500, "cancelled", "Operation cancelled", "Architecture operation was cancelled");
                }});
        }
    }

    drogon::HttpResponsePtr architectureProblemResponse(const arch::CatalogErrors &errors) {
        return present(errors);
    }
    drogon::HttpResponsePtr architectureProblemResponse(const arch::EnvironmentErrors &errors) {
        return present(errors);
    }
    drogon::HttpResponsePtr architectureProblemResponse(const arch::SynthesisErrors &errors) {
        return present(errors);
    }
}
