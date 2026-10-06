#ifndef DSSP_API_V1_CONTROLLERS_ARCHITECTUREDISPATCH_H
#define DSSP_API_V1_CONTROLLERS_ARCHITECTUREDISPATCH_H

#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <dsspapiv1/ArchitectureErrors.h>
#include <dsspapiv1/ArchitectureMapping.h>
#include <dsspapiv1/RequestCodec.h>
#include <dsspapiv1/ResponseCodec.h>
#include <dssparch/ArchitectureCatalog.h>
#include <dssphttp/RequestLifetime.h>
#include <dssphttp/Response.h>

namespace dssp::api_v1::detail {

    inline std::string unknownArchitecture(std::string_view id) {
        return "Unsupported architecture: " + std::string(id);
    }
    inline drogon::HttpResponsePtr unknownProcessArchitecture(std::string_view id) {
        return http::problemResponse(http::ValidationError({{"/context/arch", "unsupported", unknownArchitecture(id)}}));
    }
    inline arch::CatalogOptions catalogOptions(const drogon::HttpRequest &request) {
        return {request.getHeader("accept-language")};
    }
    struct DtoPresenter {
        template <class T>
        auto operator()(T value) const { return toDto(std::move(value)); }
    };

    template <class T, class Errors, class Present = DtoPresenter>
    void respond(std::expected<T, Errors> result, const http::Callback &callback, Present present = {}) {
        if (!result) {
            callback(architectureProblemResponse(result.error()));
            return;
        }
        callback(http::jsonResponse(toJson(present(std::move(*result)))));
    }
    template <class Parameters, class Method, class Present = DtoPresenter>
    void catalog(const arch::ArchitectureCatalog &architectures, const http::Callback &callback, std::string_view id, Parameters parameters, Method method, Present present = {}) {
        const auto selected = architectures.find(id);
        if (!selected) {
            callback(architectureProblemResponse(arch::CatalogErrors{arch::ResourceNotFoundError(unknownArchitecture(id))}));
            return;
        }
        respond(std::invoke(method, *selected, parameters), callback, std::move(present));
    }
    template <class Method, class Present = DtoPresenter>
    void catalogMedia(const arch::ArchitectureCatalog &architectures, const drogon::HttpRequestPtr &request, const http::Callback &callback, std::string_view id, std::string singer, Method method, Present present = {}) {
        auto query = mediaQuery(*request);
        if (!query) {
            callback(http::problemResponse(widen_errors<http::RequestErrors>(std::move(query.error()))));
            return;
        }
        catalog(architectures, callback, id, arch::SingerMediaQuery{{std::move(singer), catalogOptions(*request)}, toArchitecture(std::move(*query))}, method, std::move(present));
    }

    // The selected implementation, callback and request context outlive the
    // handler. No controller, stack reference or raw request is captured.
    // Backend tasks must release their callback when finished or cancelled.
    class AsyncReply {
    public:
        AsyncReply(http::Callback callback, http::RequestContext context, arch::ArchitecturePtr implementation) : callback_(std::move(callback)), context_(std::move(context)), implementation_(std::move(implementation)) {}
        bool claim() { return !completed_.test_and_set(std::memory_order_acq_rel) && !context_.stopRequested(); }
        void send(const drogon::HttpResponsePtr &response) const {
            if (!context_.stopRequested()) {
                callback_(response);
            }
        }

    private:
        const http::Callback callback_;
        const http::RequestContext context_;
        const arch::ArchitecturePtr implementation_;
        std::atomic_flag completed_ = ATOMIC_FLAG_INIT;
    };
    template <class Response, class Output>
    arch::Completion<Output> completion(http::Callback callback, http::RequestContext context, arch::ArchitecturePtr implementation = {}) {
        const auto reply = std::make_shared<AsyncReply>(std::move(callback), std::move(context), std::move(implementation));
        return [reply](arch::SynthesisResult<Output> result) {
            if (!reply->claim()) {
                return;
            }
            if (!result) {
                reply->send(architectureProblemResponse(result.error()));
                return;
            }
            reply->send(http::jsonResponse(toJson(Response{toDto(std::move(*result))})));
        };
    }
    template <class Request, class Response, class Output, class Start>
    void process(const drogon::HttpRequestPtr &request, http::Callback callback, const std::shared_ptr<http::RequestLifetimeRegistry> &lifetime, const arch::ArchitectureCatalog &architectures, Start start) {
        const auto context = lifetime->track(*request);
        if (context.stopRequested()) {
            return;
        }
        auto input = request->as<Decoded<Request>>();
        if (context.stopRequested()) {
            return;
        }
        if (!input) {
            callback(http::problemResponse(input.error()));
            return;
        }
        const auto selected = architectures.find(input->context.arch);
        if (!selected) {
            callback(unknownProcessArchitecture(input->context.arch));
            return;
        }
        auto parameters = toArchitecture(std::move(*input));
        if (context.stopRequested()) {
            return;
        }
        std::invoke(start, *selected, std::move(parameters), context.cancellation(), completion<Response, Output>(std::move(callback), context, selected));
    }

}

#endif // DSSP_API_V1_CONTROLLERS_ARCHITECTUREDISPATCH_H
