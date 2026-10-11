#ifndef DSSP_API_V1_CONTROLLERS_EXTRACTIONDISPATCH_H
#define DSSP_API_V1_CONTROLLERS_EXTRACTIONDISPATCH_H

#include <exception>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

#include <dsspapiv1/AsyncReply.h>
#include <dsspapiv1/ExtractionErrors.h>
#include <dsspapiv1/ExtractionMapping.h>
#include <dsspapiv1/RequestCodec.h>
#include <dsspapiv1/ResponseCodec.h>
#include <dsspextraction/Service.h>
#include <dsspresource/Async.h>

namespace dssp::api_v1::detail {
    template <class T, class Map>
    void extractionCatalog(extraction::CatalogResult<T> result, const http::Callback &callback, const http::RequestLog &context, Map map) {
        if (!result) callback(extractionProblemResponse(result.error(), context));
        else callback(http::jsonResponse(toJson(std::invoke(map, std::move(*result))), context));
    }

    template <class T>
    void extractionCatalog(extraction::CatalogResult<T> result, const http::Callback &callback, const http::RequestLog &context) {
        extractionCatalog(std::move(result), callback, context, [](T value) { return toDto(std::move(value)); });
    }

    template <class Response, class Output, class Request = dto::ExtractionRequest, class Start>
    drogon::Task<> extract(drogon::HttpRequestPtr request, http::Callback callback, std::shared_ptr<http::RequestLifetimeRegistry> lifetime, std::shared_ptr<extraction::Service> service, Start start, std::string_view operation) {
        const auto context = lifetime->track(*request);
        if (context.stopRequested()) co_return;
        auto input = request->as<Decoded<Request>>();
        if (context.stopRequested()) co_return;
        if (!input) {
            callback(http::problemResponse(input.error(), context.logContext()));
            co_return;
        }

        const auto reply = std::make_shared<AsyncReply>(std::move(callback), context, service, operation);
        auto parameters = toExtraction(std::move(*input));
        auto result = co_await resource::awaitCallback<extraction::ExtractionResult<Output>>([service, parameters = std::move(parameters), context, start](auto complete) mutable {
            std::invoke(start, *service, std::move(parameters), context.cancellation(), std::move(complete));
        });
        if (!reply->claim()) co_return;
        try {
            if (!result) reply->send(extractionProblemResponse(result.error(), context.logContext()));
            else reply->send(http::jsonResponse(toJson(Response{toDto(std::move(*result))}), context.logContext()));
        } catch (const std::exception &error) {
            const http::RequestErrors failure{http::HttpBoundaryError("Cannot encode an extraction HTTP response", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"Drogon/JsonCpp", "encode extraction response", "exception"}))};
            reply->send(http::problemResponse(failure, context.logContext()));
        }
    }
}

#endif // DSSP_API_V1_CONTROLLERS_EXTRACTIONDISPATCH_H
