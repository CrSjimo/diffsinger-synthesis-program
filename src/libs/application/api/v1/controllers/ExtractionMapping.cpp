#include "ExtractionMapping.h"

#include <utility>

namespace dssp::api_v1 {
    extraction::Input toExtraction(dto::ExtractionRequest request) { return {std::move(request.extractor), std::move(request.input.audioUrl)}; }
    extraction::SeparationInput toExtraction(dto::SeparationExtractionRequest request) {
        return {{std::move(request.extractor), std::move(request.input.audioUrl)}, request.acceptableFormats ? std::move(*request.acceptableFormats) : std::vector<std::string>{}, request.acceptableSchemes ? std::move(*request.acceptableSchemes) : std::vector<std::string>{}};
    }
    dto::ExtractorInfo toDto(extraction::ExtractorInfo info) {
        return {std::move(info.id), std::move(info.name), info.sampleRate, std::vector<std::string>{"audio/wav"}, std::vector<std::string>{"data"}, std::vector<std::uint32_t>{info.sampleRate}};
    }
    dto::SeparationExtractorInfo toSeparationDto(extraction::ExtractorInfo info) {
        std::vector<dto::SeparationTrackInfo> tracks;
        tracks.reserve(info.tracks.size());
        for (auto &track : info.tracks) tracks.push_back({std::move(track.name)});
        return {toDto(std::move(info)), std::move(tracks)};
    }
    dto::ExtractorList toDto(extraction::ExtractorList list) {
        dto::ExtractorList result;
        for (auto &info : list.note) result.note.push_back(toDto(std::move(info)));
        for (auto &info : list.tempo) result.tempo.push_back(toDto(std::move(info)));
        for (auto &info : list.pitch) result.pitch.push_back(toDto(std::move(info)));
        for (auto &info : list.separation) result.separation.push_back(toSeparationDto(std::move(info)));
        return result;
    }
    dto::NotesOutput<dto::ExtractedNote> toDto(extraction::NoteOutput output) {
        dto::NotesOutput<dto::ExtractedNote> result;
        result.notes.reserve(output.notes.size());
        for (const auto &note : output.notes) result.notes.push_back({{note.gap, note.duration}, note.cent});
        return result;
    }
    dto::PitchExtractionOutput toDto(extraction::PitchOutput output) {
        dto::PitchExtractionOutput result{{}, output.sampleRate};
        result.segments.reserve(output.segments.size());
        for (auto &segment : output.segments) result.segments.push_back({segment.gap, std::move(segment.pitch)});
        return result;
    }
    dto::TempoExtractionOutput toDto(extraction::TempoOutput output) {
        dto::TempoExtractionOutput result;
        for (const auto &beat : output.beats) result.beats.push_back({beat.position, beat.downbeat});
        return result;
    }
    dto::SeparationExtractionOutput toDto(extraction::SeparationOutput output) {
        dto::SeparationExtractionOutput result;
        for (auto &track : output.tracks) result.tracks.push_back({std::move(track.name), std::move(track.audioUrl)});
        return result;
    }
}
