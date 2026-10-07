#ifndef DSSP_API_V1_CONTROLLERS_ARCHITECTUREDISPATCH_H
#define DSSP_API_V1_CONTROLLERS_ARCHITECTUREDISPATCH_H

#include <atomic>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <stdcorelib/support/logging.h>

#include <dsspapiv1/ArchitectureErrors.h>
#include <dsspapiv1/ArchitectureMapping.h>
#include <dsspapiv1/RequestCodec.h>
#include <dsspapiv1/ResponseCodec.h>
#include <dssparch/ArchitectureCatalog.h>
#include <dssphttp/RequestLifetime.h>
#include <dssphttp/Response.h>

namespace dssp::api_v1::detail {

    inline stdc::LogCategory dispatchLog("dssp.api_v1.ArchitectureDispatch");

    inline std::string unknownArchitecture(std::string_view id) {
        return "Unsupported architecture: " + std::string(id);
    }
    inline drogon::HttpResponsePtr unknownProcessArchitecture(std::string_view id, const http::RequestLog &context = {}) {
        return http::problemResponse(http::ValidationError({{"/context/arch", "unsupported", unknownArchitecture(id)}}), context);
    }
    inline arch::CatalogOptions catalogOptions(const drogon::HttpRequest &request) {
        return {request.getHeader("accept-language")};
    }
    struct DtoPresenter {
        template <class T>
        auto operator()(T value) const { return toDto(std::move(value)); }
    };

    template <class T, class Errors, class Present = DtoPresenter>
    void respond(std::expected<T, Errors> result, const http::Callback &callback, Present present = {}, const http::RequestLog &context = {}) {
        if (!result) {
            callback(architectureProblemResponse(result.error(), context));
            return;
        }
        callback(http::jsonResponse(toJson(present(std::move(*result))), context));
    }
    template <class Parameters, class Method, class Present = DtoPresenter>
    void catalog(const arch::ArchitectureCatalog &architectures, const http::Callback &callback, std::string_view id, Parameters parameters, Method method, Present present = {}, const http::RequestLog &context = {}) {
        const auto selected = architectures.find(id);
        if (!selected) {
            callback(architectureProblemResponse(arch::CatalogErrors{arch::ResourceNotFoundError(unknownArchitecture(id))}, context));
            return;
        }
        respond(std::invoke(method, *selected, parameters), callback, std::move(present), context);
    }
    template <class Method, class Present = DtoPresenter>
    void catalogMedia(const arch::ArchitectureCatalog &architectures, const drogon::HttpRequestPtr &request, const http::Callback &callback, std::string_view id, std::string singer, Method method, Present present = {}) {
        auto query = mediaQuery(*request);
        if (!query) {
            callback(http::problemResponse(widen_errors<http::RequestErrors>(std::move(query.error())), http::requestLog(*request)));
            return;
        }
        catalog(architectures, callback, id, arch::SingerMediaQuery{{std::move(singer), catalogOptions(*request)}, toArchitecture(std::move(*query))}, method, std::move(present), http::requestLog(*request));
    }

    // The selected implementation, callback and request context outlive the
    // handler. No controller, stack reference or raw request is captured.
    // Backend tasks must release their callback when finished or cancelled.
    class AsyncReply {
    public:
        AsyncReply(http::Callback callback, http::RequestContext context, arch::ArchitecturePtr implementation, std::string_view operation) : callback_(std::move(callback)), context_(std::move(context)), implementation_(std::move(implementation)), operation_(operation) {}
        bool claim() {
            if (completed_.test_and_set(std::memory_order_acq_rel)) {
                dispatchLog.stdcWarning("%1 operation=%2 discarded a duplicate backend completion", context_.logContext().description(), operation_);
                return false;
            }
            if (context_.stopRequested()) {
                return false;
            }
            return true;
        }
        void send(const drogon::HttpResponsePtr &response) const {
            if (!context_.stopRequested()) {
                // This callback now runs on an inference worker. Drogon's
                // synchronous exception handler cannot cover this boundary.
                try {
                    callback_(response);
                } catch (const std::exception &error) {
                    dispatchLog.stdcCritical("%1 operation=%2 HTTP response callback failed: %3", context_.logContext().description(), operation_, error.what());
                }
            }
        }

    private:
        const http::Callback callback_;
        const http::RequestContext context_;
        const arch::ArchitecturePtr implementation_;
        const std::string operation_;
        std::atomic_flag completed_ = ATOMIC_FLAG_INIT;
    };
    template <class Response, class Output>
    arch::Completion<Output> completion(http::Callback callback, http::RequestContext context, arch::ArchitecturePtr implementation = {}, std::string_view operation = "synthesis") {
        const auto logContext = context.logContext();
        const auto reply = std::make_shared<AsyncReply>(std::move(callback), std::move(context), std::move(implementation), operation);
        return [reply, logContext](arch::SynthesisResult<Output> result) {
            if (!reply->claim()) {
                return;
            }
            try {
                if (!result) {
                    reply->send(architectureProblemResponse(result.error(), logContext));
                    return;
                }
                reply->send(http::jsonResponse(toJson(Response{toDto(std::move(*result))}), logContext));
            } catch (const std::exception &error) {
                const http::RequestErrors failure{http::HttpBoundaryError("Cannot encode an asynchronous HTTP response", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"Drogon/JsonCpp", "encode asynchronous response", "exception"}))};
                reply->send(http::problemResponse(failure, logContext));
            }
        };
    }
    template <class Request, class Response, class Output, class Start>
    void process(const drogon::HttpRequestPtr &request, http::Callback callback, const std::shared_ptr<http::RequestLifetimeRegistry> &lifetime, const arch::ArchitectureCatalog &architectures, Start start, std::string_view operation = "synthesis") {
        const auto context = lifetime->track(*request);
        if (context.stopRequested()) {
            return;
        }
        auto input = request->as<Decoded<Request>>();
        if (context.stopRequested()) {
            return;
        }
        if (!input) {
            callback(http::problemResponse(input.error(), context.logContext()));
            return;
        }
        const auto selected = architectures.find(input->context.arch);
        if (!selected) {
            callback(unknownProcessArchitecture(input->context.arch, context.logContext()));
            return;
        }
        auto parameters = toArchitecture(std::move(*input));
        if (context.stopRequested()) {
            return;
        }
        dispatchLog.stdcTrace("%1 operation=%2 dispatching to architecture=%3", context.logContext().description(), operation, selected->id());
        std::invoke(start, *selected, std::move(parameters), context.cancellation(), completion<Response, Output>(std::move(callback), context, selected, operation));
    }

}

#endif // DSSP_API_V1_CONTROLLERS_ARCHITECTUREDISPATCH_H
