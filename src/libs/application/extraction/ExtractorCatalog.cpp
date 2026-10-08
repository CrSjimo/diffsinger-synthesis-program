#include "ExtractorCatalog.h"

#include <cmath>
#include <exception>

#include <stdcorelib/support/logging.h>
#include <synthrt/Core/SynthUnit.h>

#include <dsspinference/Display.h>
#include <dsspinference/ResourceIdentity.h>
#include <dsspinference/ParallelismPolicy.h>

namespace dssp::extraction {
    namespace {
        namespace Note = otter::Api::Note::L1;
        namespace F0 = otter::Api::F0::L1;
        stdc::LogCategory log("dssp.extraction.ExtractorCatalog");

        template <class Schema, class Create>
        std::expected<ExtractorRecord, inference::ResourceError> read(srt::InferenceSpec &spec, Kind kind, inference::InferencePool &pool, Create create) {
            const auto *schema = dynamic_cast<const Schema *>(spec.exports());
            if (!schema || !spec.configuration() || schema->interface() != spec.interface() || schema->level() != spec.level() || schema->variant() != spec.variant() || schema->sampleRate <= 0 || schema->channelCount != 1 || !std::isfinite(schema->maxSegmentDuration) || schema->maxSegmentDuration < 0) {
                return std::unexpected{inference::ResourceError("Analyzer has an unsupported typed contract, format or duration limit")};
            }
            ExtractorRecord record;
            record.kind = kind;
            record.id = inference::resourceReference(spec);
            record.name = spec.name();
            record.package = spec.package();
            record.spec = &spec;
            record.sampleRate = schema->sampleRate;
            record.maximumDuration = schema->maxSegmentDuration;
            record.pool = &pool;
            if constexpr (requires { schema->interval; }) {
                if (!std::isfinite(schema->interval) || schema->interval <= 0 || !std::isfinite(1.0 / schema->interval)) {
                    return std::unexpected{inference::ResourceError("F0 analyzer has an invalid output interval")};
                }
                record.outputInterval = schema->interval;
            }
            inference::InferenceDefinition definition;
            definition.key = record.id + "/default";
            definition.label = record.id;
            definition.packages = {record.package};
            definition.parallelSafe = inference::permitsParallelExecution(spec);
            definition.create = [&spec, create]() -> std::expected<std::unique_ptr<srt::InferenceExecutive>, inference::ExecutionError> {
                auto created = inference::nativeCall("Otter createAnalyzer", [&] { return create(spec); });
                if (!created) return std::unexpected{std::move(created.error())};
                return std::move(*created);
            };
            auto resource = pool.bind(std::move(definition));
            if (!resource) return std::unexpected{inference::ResourceError("Cannot bind analyzer resource", resource.error().clone())};
            record.resource = std::move(*resource);
            return record;
        }
        CatalogResult<ExtractorInfo> present(const ExtractorRecord &record, std::string_view acceptLanguage) {
            auto name = inference::localize(record.name, acceptLanguage, record.id);
            if (!name) return fail<CatalogErrors>(OperationError("Cannot localize extractor " + record.id, name.error().clone()));
            return ExtractorInfo{record.id, std::move(*name), static_cast<std::uint32_t>(record.sampleRate)};
        }
    }

    std::expected<std::shared_ptr<const ExtractorCatalog>, inference::InitializationError> ExtractorCatalog::discover(std::shared_ptr<inference::Environment> environment) {
        if (!environment) return std::unexpected{inference::InitializationError("Extraction requires an inference environment")};
        auto catalog = std::shared_ptr<ExtractorCatalog>(new ExtractorCatalog(std::move(environment)));
        auto notePool = catalog->environment_->inferences("extraction-note");
        auto pitchPool = catalog->environment_->inferences("extraction-pitch");
        if (!notePool) return std::unexpected{std::move(notePool.error())};
        if (!pitchPool) return std::unexpected{std::move(pitchPool.error())};
        try {
            auto *category = dynamic_cast<srt::InferenceCategory *>(catalog->environment_->unit().category(srt::InferenceCategory::NAME));
            if (!category) return std::unexpected{inference::InitializationError("Synthrt inference category is unavailable")};
            std::size_t notes = 0, pitches = 0, rejected = 0;
            for (auto *spec : category->inferences()) {
                if (spec->variant() == "stub" || spec->level() != 1) continue;
                std::expected<ExtractorRecord, inference::ResourceError> record = std::unexpected{inference::ResourceError("Unsupported analysis contract")};
                if (spec->interface() == Note::API_INTERFACE) {
                    record = read<Note::NoteSchema>(*spec, Kind::Note, **notePool, Note::createAnalyzer);
                } else if (spec->interface() == F0::API_INTERFACE) {
                    record = read<F0::F0Schema>(*spec, Kind::Pitch, **pitchPool, F0::createAnalyzer);
                } else {
                    continue;
                }
                if (!record) {
                    ++rejected;
                    log.stdcWarning("Skipping extractor %1: %2", inference::resourceReference(*spec), record.error().formatChain());
                    continue;
                }
                notes += record->kind == Kind::Note;
                pitches += record->kind == Kind::Pitch;
                log.stdcTrace("Registered extractor %1 rate=%2 maximum_seconds=%3 output_interval=%4", record->id, record->sampleRate, record->maximumDuration, record->outputInterval);
                const auto id = record->id;
                catalog->records_.emplace(id, std::move(*record));
            }
            log.stdcInfo("Extractor catalogue ready: %1 note extractors, %2 pitch extractors, %3 rejected declarations", notes, pitches, rejected);
            return std::shared_ptr<const ExtractorCatalog>(std::move(catalog));
        } catch (const std::exception &error) {
            return std::unexpected{inference::InitializationError("Cannot enumerate Otter analyzers", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"otter/synthrt", "enumerate analyzers", "exception"}))};
        }
    }
    const ExtractorRecord *ExtractorCatalog::find(Kind kind, std::string_view id) const {
        const auto found = records_.find(id);
        return found != records_.end() && found->second.kind == kind ? &found->second : nullptr;
    }
    CatalogResult<ExtractorInfo> ExtractorCatalog::metadata(Kind kind, std::string_view id, std::string_view acceptLanguage) const {
        const auto *record = find(kind, id);
        if (!record) return fail<CatalogErrors>(ResourceNotFoundError("Extractor does not exist for the requested type: " + std::string(id)));
        return present(*record, acceptLanguage);
    }
    CatalogResult<ExtractorList> ExtractorCatalog::list(std::string_view acceptLanguage) const {
        ExtractorList result;
        for (const auto &[id, record] : records_) {
            auto info = present(record, acceptLanguage);
            if (!info) return std::unexpected{std::move(info.error())};
            (record.kind == Kind::Note ? result.note : result.pitch).push_back(std::move(*info));
        }
        return result;
    }
}
