#ifndef DSSP_API_V1_DTO_REQUESTS_H
#define DSSP_API_V1_DTO_REQUESTS_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <json/value.h>

namespace dssp::api_v1::dto {

    // Extension payloads intentionally remain arbitrary JSON. Architecture-specific
    // interpretation and defaulting belong to the future service layer.
    struct Singer {
        std::string id;
        Json::Value extra;
    };

    struct SingleSingerContext {
        std::string arch;
        Json::Value archExtra;
        Singer singer;
    };

    struct MultiSingerContext {
        std::string arch;
        Json::Value archExtra;
        std::vector<Singer> singers;
    };

    struct EnvTagRequest {
        MultiSingerContext context;
    };

    struct Lyric {
        std::string lyric;
        std::string language;
    };

    struct PronunciationInput {
        std::vector<Lyric> notes;
    };

    struct PronunciationRequest {
        SingleSingerContext context;
        PronunciationInput input;
    };

    struct PronunciationNote {
        std::string pronunciation;
        std::string language;
        std::optional<std::int64_t> syllableSliceStart;
        std::optional<std::int64_t> syllableSliceEnd;
    };

    struct PhonemeInput {
        std::vector<PronunciationNote> notes;
    };

    struct PhonemeRequest {
        SingleSingerContext context;
        PhonemeInput input;
    };

    enum class NoteKind {
        Normal,
        Slur
    };

    struct NotePosition {
        double gap = 0;
        double duration = 0;
    };

    struct DurationInputPhoneme {
        std::string token;
        bool onset = false;
        std::string language;
    };

    struct ParameterInputPhoneme : DurationInputPhoneme {
        double start = 0;
    };

    template <class Phoneme>
    struct Note {
        NotePosition position;
        std::uint32_t cent = 0;
        std::string pronunciation;
        std::string language;
        std::vector<Phoneme> phonemes;
        NoteKind kind = NoteKind::Normal;
    };

    using DurationNote = Note<DurationInputPhoneme>;
    using ParameterNote = Note<ParameterInputPhoneme>;
    using Mix = std::vector<std::vector<double>>;
    template <class NoteType>
    struct PieceInput {
        double pieceDuration = 0;
        std::vector<NoteType> notes;
        Mix mix;
        double mixSampleRate = 1;
    };

    using DurationInput = PieceInput<DurationNote>;
    struct DurationRequest {
        MultiSingerContext context;
        DurationInput input;
    };

    struct ParameterRetake {
        std::uint64_t position = 0;
        std::uint64_t length = 0;
    };

    struct Parameter {
        std::vector<double> values;
        double sampleRate = 1;
        std::optional<ParameterRetake> retake;
    };

    struct AudioParameter {
        double sampleRate = 1;
        std::optional<std::vector<double>> values;
    };

    struct ParameterInput : PieceInput<ParameterNote> {
        std::map<std::string, Parameter> parameters;
    };

    struct AudioInput : PieceInput<ParameterNote> {
        std::map<std::string, AudioParameter> parameters;
    };

    struct ParameterRequest {
        MultiSingerContext context;
        ParameterInput input;
    };

    struct AudioRequest {
        MultiSingerContext context;
        AudioInput input;
        std::optional<std::vector<std::string>> acceptableFormats;
        std::optional<std::vector<std::string>> acceptableSchemes;
    };

    struct ExtractionInput {
        std::string audioUrl;
    };

    struct ExtractionRequest {
        std::string extractor;
        ExtractionInput input;
    };

    struct SeparationExtractionRequest : ExtractionRequest {
        std::optional<std::vector<std::string>> acceptableFormats;
        std::optional<std::vector<std::string>> acceptableSchemes;
    };

    struct MediaQuery {
        std::vector<std::string> acceptableFormats;
        std::vector<std::string> acceptableSchemes;
    };

}

#endif // DSSP_API_V1_DTO_REQUESTS_H
