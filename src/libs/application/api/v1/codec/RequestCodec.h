#ifndef DSSP_API_V1_CODEC_REQUESTCODEC_H
#define DSSP_API_V1_CODEC_REQUESTCODEC_H

#include <dsspapiv1/Requests.h>
#include <dssphttp/JsonPayload.h>

namespace dssp::api_v1 {
    template <class T>
    std::expected<T, http::DtoErrors> fromJson(const Json::Value &) = delete;
    template <>
    std::expected<dto::EnvTagRequest, http::DtoErrors> fromJson<dto::EnvTagRequest>(const Json::Value &);
    template <>
    std::expected<dto::PronunciationRequest, http::DtoErrors> fromJson<dto::PronunciationRequest>(const Json::Value &);
    template <>
    std::expected<dto::PhonemeRequest, http::DtoErrors> fromJson<dto::PhonemeRequest>(const Json::Value &);
    template <>
    std::expected<dto::DurationRequest, http::DtoErrors> fromJson<dto::DurationRequest>(const Json::Value &);
    template <>
    std::expected<dto::ParameterRequest, http::DtoErrors> fromJson<dto::ParameterRequest>(const Json::Value &);
    template <>
    std::expected<dto::AudioRequest, http::DtoErrors> fromJson<dto::AudioRequest>(const Json::Value &);
    template <>
    std::expected<dto::ExtractionRequest, http::DtoErrors> fromJson<dto::ExtractionRequest>(const Json::Value &);

    std::expected<dto::MediaQuery, http::DtoErrors> mediaQuery(const drogon::HttpRequest &request);

    template <class T>
    using Decoded = std::expected<T, http::RequestErrors>;
    template <class T>
    Decoded<T> fromHttp(const drogon::HttpRequest &request) {
        auto json = http::readJsonPayload(request);
        if (!json) {
            return std::unexpected{widen_errors<http::RequestErrors>(std::move(json.error()))};
        }
        auto dto = fromJson<T>(*json);
        if (!dto) {
            return std::unexpected{widen_errors<http::RequestErrors>(std::move(dto.error()))};
        }
        return std::move(*dto);
    }
}

// Drogon's custom request conversion mechanism; no throwing conversion is used.
namespace drogon {
    template <>
    inline dssp::api_v1::Decoded<dssp::api_v1::dto::EnvTagRequest> fromRequest<dssp::api_v1::Decoded<dssp::api_v1::dto::EnvTagRequest>>(const HttpRequest &request) {
        return dssp::api_v1::fromHttp<dssp::api_v1::dto::EnvTagRequest>(request);
    }
    template <>
    inline dssp::api_v1::Decoded<dssp::api_v1::dto::PronunciationRequest> fromRequest<dssp::api_v1::Decoded<dssp::api_v1::dto::PronunciationRequest>>(const HttpRequest &request) {
        return dssp::api_v1::fromHttp<dssp::api_v1::dto::PronunciationRequest>(request);
    }
    template <>
    inline dssp::api_v1::Decoded<dssp::api_v1::dto::PhonemeRequest> fromRequest<dssp::api_v1::Decoded<dssp::api_v1::dto::PhonemeRequest>>(const HttpRequest &request) {
        return dssp::api_v1::fromHttp<dssp::api_v1::dto::PhonemeRequest>(request);
    }
    template <>
    inline dssp::api_v1::Decoded<dssp::api_v1::dto::DurationRequest> fromRequest<dssp::api_v1::Decoded<dssp::api_v1::dto::DurationRequest>>(const HttpRequest &request) {
        return dssp::api_v1::fromHttp<dssp::api_v1::dto::DurationRequest>(request);
    }
    template <>
    inline dssp::api_v1::Decoded<dssp::api_v1::dto::ParameterRequest> fromRequest<dssp::api_v1::Decoded<dssp::api_v1::dto::ParameterRequest>>(const HttpRequest &request) {
        return dssp::api_v1::fromHttp<dssp::api_v1::dto::ParameterRequest>(request);
    }
    template <>
    inline dssp::api_v1::Decoded<dssp::api_v1::dto::AudioRequest> fromRequest<dssp::api_v1::Decoded<dssp::api_v1::dto::AudioRequest>>(const HttpRequest &request) {
        return dssp::api_v1::fromHttp<dssp::api_v1::dto::AudioRequest>(request);
    }
    template <>
    inline dssp::api_v1::Decoded<dssp::api_v1::dto::ExtractionRequest> fromRequest<dssp::api_v1::Decoded<dssp::api_v1::dto::ExtractionRequest>>(const HttpRequest &request) {
        return dssp::api_v1::fromHttp<dssp::api_v1::dto::ExtractionRequest>(request);
    }
}

#endif // DSSP_API_V1_CODEC_REQUESTCODEC_H
