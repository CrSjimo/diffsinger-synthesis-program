#include "ExtractionMapping.h"

#include <utility>

namespace dssp::api_v1 {
    extraction::Input toExtraction(dto::ExtractionRequest request) { return {std::move(request.extractor), std::move(request.input.audioUrl)}; }
    dto::ExtractorInfo toDto(extraction::ExtractorInfo info) {
        dto::ExtractorInfo result{std::move(info.id), std::move(info.name), info.sampleRate, std::vector<std::string>{"audio/wav"}, std::vector<std::string>{"data"}, std::vector<std::uint32_t>{info.sampleRate}};
        if (!info.tracks.empty()) {
            result.tracks.emplace();
            for (auto &track : info.tracks) result.tracks->push_back({std::move(track.name), std::move(track.description)});
        }
        return result;
    }
    dto::ExtractorList toDto(extraction::ExtractorList list) {
        dto::ExtractorList result;
        for (auto &info : list.note) result.note.push_back(toDto(std::move(info)));
        for (auto &info : list.pitch) result.pitch.push_back(toDto(std::move(info)));
        for (auto &info : list.separation) result.separation.push_back(toDto(std::move(info)));
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
    dto::SeparationExtractionOutput toDto(extraction::SeparationOutput output) {
        dto::SeparationExtractionOutput result;
        for (auto &track : output.tracks) result.tracks.push_back({std::move(track.name), std::move(track.audioUrl)});
        return result;
    }
}
