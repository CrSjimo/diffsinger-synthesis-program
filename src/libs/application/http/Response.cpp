#include "Response.h"

#include <stdcorelib/support/logging.h>

namespace dssp::http {
    namespace {
        stdc::LogCategory log("dssp.http.Response");
        drogon::HttpResponsePtr problem(int status, std::string_view type, std::string_view title, std::string detail, const std::vector<FieldIssue> *issues = nullptr) {
            Json::Value body;
            body["type"] = std::string("urn:dssp:error:") + std::string(type);
            body["title"] = std::string(title);
            body["status"] = status;
            body["detail"] = std::move(detail);
            if (issues) {
                body["errors"] = Json::Value(Json::arrayValue);
                for (const auto &issue : *issues) {
                    Json::Value value;
                    value["pointer"] = issue.pointer;
                    value["errorType"] = issue.errorType;
                    value["detail"] = issue.detail;
                    body["errors"].append(std::move(value));
                }
            }
            auto response = jsonResponse(body);
            if (response->statusCode() != drogon::k200OK) {
                return response;
            }
            response->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
            response->setContentTypeString("application/problem+json");
            return response;
        }
    }
    drogon::HttpResponsePtr jsonResponse(const Json::Value &body) {
        try {
            auto response = drogon::HttpResponse::newHttpJsonResponse(body);
            // Drogon serializes JSON lazily. Materialize it here to keep JSONcpp
            // exceptions inside the adapter rather than the send callback.
            (void)response->body();
            return response;
        } catch (const Json::Exception &error) {
            // JSONcpp is the only throwing operation in this adapter.
            log.stdcCritical("JSON response serialization failed: %1", error.what());
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setStatusCode(drogon::k500InternalServerError);
            response->setContentTypeString("application/problem+json");
            response->setBody(R"({"type":"urn:dssp:error:serialization","title":"Internal server error","status":500,"detail":"Response serialization failed"})");
            return response;
        }
    }
    drogon::HttpResponsePtr problemResponse(const ValidationError &error) {
        // Do not log request contents, extension payloads, URLs or parser excerpts.
        log.stdcDebug("Request schema validation failed (%1 fields)", error.issues().size());
        return problem(422, "validation", "Invalid request", error.message(), &error.issues());
    }
    drogon::HttpResponsePtr problemResponse(const RequestErrors &errors) {
        const auto visitor = Overload{
            [](const MalformedJsonError &e) {
                log.stdcDebug("Malformed request JSON");
                return problem(400, "malformed-json", "Malformed JSON", e.message());
            },
            [](const UnsupportedPayloadError &e) {
                log.stdcDebug("Unsupported HTTP payload");
                return problem(415, "unsupported-payload", "Unsupported payload", e.message());
            },
            [](const ValidationError &e) { return problemResponse(e); },
            [](const HttpBoundaryError &e) {
                log.stdcCritical("HTTP adapter failure: %1", e.formatChain());
                return problem(500, "http-boundary", "Internal server error", "HTTP payload processing failed");
            }};
        return errors.visit(visitor);
    }
    drogon::HttpResponsePtr problemResponse(int status, std::string_view type, std::string_view title, std::string detail) {
        return problem(status, type, title, std::move(detail));
    }
}
