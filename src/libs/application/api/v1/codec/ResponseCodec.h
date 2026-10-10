#ifndef DSSP_API_V1_CODEC_RESPONSECODEC_H
#define DSSP_API_V1_CODEC_RESPONSECODEC_H

#include <concepts>

#include <dsspapiv1/Responses.h>

namespace dssp::api_v1 {

    Json::Value toJson(const dto::ApiVersion &value);
    Json::Value toJson(const dto::ApplicationInfo &value);
    Json::Value toJson(const dto::ApplicationInfoResponse &value);
    Json::Value toJson(const dto::AST &value);
    Json::Value toJson(const dto::ParameterPresentation &value);
    Json::Value toJson(const dto::ArchitectureMetadata &value);
    Json::Value toJson(const dto::GroupMetadata &value);
    Json::Value toJson(const dto::GroupLanguageInfo &value);
    Json::Value toJson(const dto::SingerInfo &value);
    Json::Value toJson(const dto::SingerAvatarResponse &value);
    Json::Value toJson(const dto::SingerBackgroundResponse &value);
    Json::Value toJson(const dto::SingerDemoAudio &value);
    Json::Value toJson(const dto::ExtractorInfo &value);
    Json::Value toJson(const dto::SeparationTrackInfo &value);
    Json::Value toJson(const dto::SeparationExtractorInfo &value);
    Json::Value toJson(const dto::ExtractorList &value);
    Json::Value toJson(const dto::EnvTagResponse &value);
    Json::Value toJson(const dto::Pronunciation &value);
    Json::Value toJson(const dto::Phoneme &value);
    Json::Value toJson(const dto::PhonemeNote &value);
    Json::Value toJson(const dto::DurationOutputPhoneme &value);
    Json::Value toJson(const dto::DurationOutputNote &value);
    Json::Value toJson(const dto::ParameterOutputParameter &value);
    Json::Value toJson(const dto::ParameterOutput &value);
    Json::Value toJson(const dto::AudioOutput &value);
    Json::Value toJson(const dto::NotePosition &value);
    Json::Value toJson(const dto::ExtractedNote &value);
    Json::Value toJson(const dto::PitchExtractionSegment &value);
    Json::Value toJson(const dto::PitchExtractionOutput &value);
    Json::Value toJson(const dto::TempoExtractionOutput &value);
    Json::Value toJson(const dto::ExtractedBeat &value);
    Json::Value toJson(const dto::SeparationTrack &value);
    Json::Value toJson(const dto::SeparationExtractionOutput &value);

    inline Json::Value toJson(const std::string &value) {
        return value;
    }
    inline Json::Value toJson(double value) {
        return value;
    }
    inline Json::Value toJson(bool value) {
        return value;
    }
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    Json::Value toJson(T value) {
        if constexpr (std::signed_integral<T>) {
            return Json::Int64(value);
        } else {
            return Json::UInt64(value);
        }
    }
    template <class T>
    Json::Value toJson(const std::vector<T> &values);
    template <class T>
    Json::Value toJson(const std::map<std::string, T> &values);
    template <class T>
    Json::Value toJson(const std::optional<T> &value);
    template <class T>
    Json::Value toJson(const dto::Response<T> &value);
    template <class T>
    Json::Value toJson(const dto::NotesOutput<T> &value);

    template <class T>
    Json::Value toJson(const std::vector<T> &values) {
        Json::Value result(Json::arrayValue);
        for (const auto &value : values) {
            result.append(toJson(value));
        }
        return result;
    }
    template <class T>
    Json::Value toJson(const std::map<std::string, T> &values) {
        Json::Value result(Json::objectValue);
        for (const auto &[key, value] : values) {
            result[key] = toJson(value);
        }
        return result;
    }
    template <class T>
    Json::Value toJson(const std::optional<T> &value) {
        return value ? toJson(*value) : Json::Value{};
    }
    template <class T>
    Json::Value toJson(const dto::Response<T> &value) {
        Json::Value result(Json::objectValue);
        result["output"] = toJson(value.output);
        return result;
    }
    template <class T>
    Json::Value toJson(const dto::NotesOutput<T> &value) {
        Json::Value result(Json::objectValue);
        result["notes"] = toJson(value.notes);
        return result;
    }

}

#endif // DSSP_API_V1_CODEC_RESPONSECODEC_H
