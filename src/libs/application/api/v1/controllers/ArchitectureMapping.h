#ifndef DSSP_API_V1_CONTROLLERS_ARCHITECTUREMAPPING_H
#define DSSP_API_V1_CONTROLLERS_ARCHITECTUREMAPPING_H

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <dsspapiv1/Responses.h>
#include <dssparch/Types.h>

namespace dssp::api_v1 {

    arch::Singer toArchitecture(dto::Singer);
    arch::SingleSingerContext toArchitecture(dto::SingleSingerContext);
    arch::MultiSingerContext toArchitecture(dto::MultiSingerContext);
    arch::Lyric toArchitecture(dto::Lyric);
    arch::PronunciationNote toArchitecture(dto::PronunciationNote);
    arch::DurationInputPhoneme toArchitecture(dto::DurationInputPhoneme);
    arch::TimedPhoneme toArchitecture(dto::ParameterInputPhoneme);
    arch::DurationNote toArchitecture(dto::DurationNote);
    arch::ParameterNote toArchitecture(dto::ParameterNote);
    arch::MultiSingerContext toArchitecture(dto::EnvTagRequest);
    arch::PronunciationInput toArchitecture(dto::PronunciationRequest);
    arch::PhonemeInput toArchitecture(dto::PhonemeRequest);
    arch::DurationInput toArchitecture(dto::DurationRequest);
    arch::ParameterInput toArchitecture(dto::ParameterRequest);
    arch::AudioInput toArchitecture(dto::AudioRequest);
    arch::MediaPreferences toArchitecture(dto::MediaQuery);

    dto::AST toDto(arch::AST);
    dto::ParameterPresentation toDto(arch::ParameterPresentation);
    dto::ArchitectureMetadata toDto(arch::ArchitectureMetadata);
    dto::GroupMetadata toDto(arch::GroupMetadata);
    dto::SingerLanguageInfo toDto(arch::SingerLanguageInfo);
    dto::SingerInfo toDto(arch::SingerInfo);
    dto::SingerDemoAudio toDto(arch::SingerDemoAudio);
    dto::Pronunciation toDto(arch::Pronunciation);
    dto::Phoneme toDto(arch::Phoneme);
    dto::PhonemeNote toDto(arch::PhonemeNote);
    dto::DurationOutputPhoneme toDto(arch::DurationOutputPhoneme);
    dto::DurationOutputNote toDto(arch::DurationOutputNote);
    dto::ParameterOutputParameter toDto(arch::ParameterOutputParameter);
    dto::ParameterOutput toDto(arch::ParameterOutput);
    dto::AudioOutput toDto(arch::AudioOutput);

    template <class T>
    auto toArchitecture(std::vector<T> values) {
        std::vector<decltype(toArchitecture(std::declval<T>()))> result;
        result.reserve(values.size());
        for (auto &value : values) {
            result.push_back(toArchitecture(std::move(value)));
        }
        return result;
    }
    template <class T>
    auto toDto(std::vector<T> values) {
        std::vector<decltype(toDto(std::declval<T>()))> result;
        result.reserve(values.size());
        for (auto &value : values) {
            result.push_back(toDto(std::move(value)));
        }
        return result;
    }
    template <class T>
    auto toDto(std::optional<T> value) {
        using Result = std::optional<decltype(toDto(std::declval<T>()))>;
        return value ? Result{toDto(std::move(*value))} : Result{};
    }
    template <class T>
    auto toDto(std::map<std::string, T> values) {
        std::map<std::string, decltype(toDto(std::declval<T>()))> result;
        for (auto &[key, value] : values) {
            result.emplace(key, toDto(std::move(value)));
        }
        return result;
    }
    template <class T>
    auto toDto(arch::NotesOutput<T> output) {
        return dto::NotesOutput<decltype(toDto(std::declval<T>()))>{toDto(std::move(output.notes))};
    }

}

#endif // DSSP_API_V1_CONTROLLERS_ARCHITECTUREMAPPING_H
