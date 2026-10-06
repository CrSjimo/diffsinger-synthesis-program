#include "SingerCatalog.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iterator>
#include <optional>
#include <utility>

#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>
#include <stdcorelib/support/logging.h>
#include <synthrt/Core/ContribImportBinding.h>
#include <wolf/Linguist/LinguistContrib.h>

#include <dssparchdiffsinger/Factory.h>
#include <dssparchdiffsinger/SingerMetadata.h>
#include <dsspinference/Display.h>
#include <dsspinference/MediaPath.h>
#include <dsspinference/ResourceIdentity.h>

namespace dssp::arch_diffsinger {
    namespace {
        stdc::LogCategory log("dssp.arch_diffsinger.SingerCatalog");
        namespace Du = ds::Api::Duration::L1;
        namespace Pi = ds::Api::Pitch::L1;
        namespace Va = ds::Api::Variance::L1;
        namespace Ac = ds::Api::Acoustic::L1;
        namespace Vo = ds::Api::Vocoder::L1;
        namespace Ds = ds::Api::DiffSinger::L1;
        using Depth = wolf::Api::Linguist::L1::Depth;

        template <class Schema, class Configuration, class Options>
        std::expected<StageRecord, inference::ResourceError> readStage(const srt::SingerSpec &singer, std::string_view role, bool required) {
            const auto imported = singer.findImport(role);
            if (!imported) {
                if (required) {
                    return std::unexpected{inference::ResourceError("Missing required stage " + std::string(role))};
                }
                return StageRecord{};
            }
            if (!imported->binding() || imported->binding()->state() != srt::ContribImportBinding::Active) {
                return std::unexpected{inference::ResourceError("Unresolved stage " + std::string(role))};
            }
            const auto &target = imported->binding()->target();
            const auto *schema = dynamic_cast<const Schema *>(target.exports());
            const auto *configuration = dynamic_cast<const Configuration *>(target.configuration());
            const auto *options = dynamic_cast<const Options *>(imported->options());
            if (!schema || !configuration || !options || target.interface() != schema->interface() || target.variant() != schema->variant() || target.level() != schema->level()) {
                return std::unexpected{inference::ResourceError("Unsupported typed stage contract for " + std::string(role))};
            }
            StageRecord result;
            result.spec = &target;
            result.options = options;
            if constexpr (requires { configuration->useSpeakerEmbedding; schema->speakers; options->speakerMapping; }) {
                result.usesSpeakers = configuration->useSpeakerEmbedding;
                if (result.usesSpeakers) {
                    result.speakers = schema->speakers;
                    result.speakerMapping = options->speakerMapping;
                    for (const auto &[speaker, embedding] : configuration->speakers) {
                        if (embedding.size() == static_cast<std::size_t>(configuration->hiddenSize) && !embedding.empty() && std::ranges::all_of(embedding, [](float value) { return std::isfinite(value); })) {
                            result.embeddedSpeakers.insert(speaker);
                        }
                    }
                }
            }
            return result;
        }

        const char *depthName(Depth depth) {
            switch (depth) {
                case Depth::Pronunciation: return "pronunciation";
                case Depth::Phonemes: return "phonemes";
                case Depth::Onsets: return "onsets";
            }
            std::unreachable();
        }

        template <class Configuration>
        void intersectPhonemes(const StageRecord &stage, std::optional<std::set<std::string>> &phonemes) {
            if (!stage.spec) {
                return;
            }
            const auto *configuration = dynamic_cast<const Configuration *>(stage.spec->configuration());
            std::set<std::string> available;
            for (const auto &[token, index] : configuration->phonemes) {
                available.insert(token);
            }
            if (!phonemes) {
                phonemes = std::move(available);
            } else {
                std::set<std::string> common;
                std::ranges::set_intersection(*phonemes, available, std::inserter(common, common.end()));
                phonemes = std::move(common);
            }
        }

        std::expected<std::pair<SingerRecord, arch::GroupMetadata>, inference::ResourceError> readSinger(const srt::SingerSpec &singer, inference::Environment &environment) {
            if (!srt::ContribSpecExtension::findFromSpec<Ds::DiffSingerPipelineExecutive>(singer)) {
                return std::unexpected{inference::ResourceError("The DiffSinger pipeline provider is unavailable")};
            }
            const auto *configuration = dynamic_cast<const Ds::DiffSingerConfiguration *>(singer.configuration());
            if (!configuration) {
                return std::unexpected{inference::ResourceError("The DiffSinger configuration is unavailable")};
            }
            auto duration = readStage<Du::DurationSchema, Du::DurationConfiguration, Du::DurationImportOptions>(singer, "singer/duration", false);
            auto pitch = readStage<Pi::PitchSchema, Pi::PitchConfiguration, Pi::PitchImportOptions>(singer, "singer/pitch", false);
            auto variance = readStage<Va::VarianceSchema, Va::VarianceConfiguration, Va::VarianceImportOptions>(singer, "singer/variance", false);
            auto acoustic = readStage<Ac::AcousticSchema, Ac::AcousticConfiguration, Ac::AcousticImportOptions>(singer, "singer/acoustic", true);
            auto vocoder = readStage<Vo::VocoderSchema, Vo::VocoderConfiguration, Vo::VocoderImportOptions>(singer, "singer/vocoder", true);
            for (const auto *stage : {&duration, &pitch, &variance, &acoustic, &vocoder}) {
                if (!*stage) {
                    return std::unexpected{stage->error()};
                }
            }
            SingerRecord record;
            record.package = singer.package();
            record.spec = &singer;
            record.ref = {srt::ContribLocator(record.package.id(), "singer", singer.locator().contributionId()), record.package.version()};
            record.info.id = inference::resourceId(singer);
            record.info.arch = Id;
            record.name = singer.name();
            const auto readMediaPath = [&singer](std::string_view field, const srt::DisplayText &resolved) -> std::expected<srt::DisplayText, inference::ResourceError> {
                const auto &declaration = singer.manifestDeclaration();
                const auto found = declaration.find(std::string(field));
                return found == declaration.end() ? std::expected<srt::DisplayText, inference::ResourceError>{resolved} : inference::preserveEmptyMediaPaths(resolved, found->second);
            };
            auto avatar = readMediaPath("avatar", singer.avatar());
            auto background = readMediaPath("background", singer.background());
            auto demoAudio = readMediaPath("demoAudio", singer.demoAudio());
            for (const auto *path : {&avatar, &background, &demoAudio}) {
                if (!*path) {
                    return std::unexpected{path->error()};
                }
            }
            record.avatar = std::move(*avatar);
            record.background = std::move(*background);
            record.demoAudio = std::move(*demoAudio);
            record.stages = {{"duration", *duration}, {"pitch", *pitch}, {"variance", *variance}, {"acoustic", *acoustic}, {"vocoder", *vocoder}};
            record.speakers = commonSpeakers(record.stages);
            const bool needsSpeakers = std::ranges::any_of(record.stages, [](const auto &entry) { return entry.second.usesSpeakers; });
            if (needsSpeakers && record.speakers.empty()) {
                return std::unexpected{inference::ResourceError("No logical speaker is valid across the singer's effective stages")};
            }
            record.permitsMixing = singerMetadata(*configuration).permitsMixing;

            std::optional<std::set<std::string>> phonemes;
            intersectPhonemes<Du::DurationConfiguration>(*duration, phonemes);
            intersectPhonemes<Pi::PitchConfiguration>(*pitch, phonemes);
            intersectPhonemes<Va::VarianceConfiguration>(*variance, phonemes);
            intersectPhonemes<Ac::AcousticConfiguration>(*acoustic, phonemes);
            if (phonemes) {
                environment.languages().setSingerPhonemes(record.ref, std::vector<std::string>(phonemes->begin(), phonemes->end()));
            }
            arch::GroupMetadata group;
            group.arch = Id;
            bool pronunciation = false;
            bool completeLanguagePipeline = false;
            Depth commonDepth = Depth::Onsets;
            Json::Value capabilities(Json::objectValue);
            const auto languageCatalog = environment.languages().catalog();
            if (const auto *entry = languageCatalog->find(record.ref)) {
                pronunciation = !entry->languages.empty();
                completeLanguagePipeline = pronunciation;
                for (const auto &language : entry->languages) {
                    const auto status = environment.languages().probe(record.ref, language.handle);
                    if (status.readiness == wolf::Readiness::Unavailable) {
                        log.stdcWarning("Singer %1 language %2 is unavailable: %3", record.info.id, language.handle, status.reason);
                        pronunciation = false;
                        completeLanguagePipeline = false;
                        continue;
                    }
                    const auto role = singer.languages().find(language.handle);
                    const auto imported = role == singer.languages().end() ? std::optional<srt::ContribImport>{} : singer.findImport(role->second);
                    if (!imported || !imported->binding()) {
                        return std::unexpected{inference::ResourceError("The language catalogue has no resolved singer binding for " + language.handle)};
                    }
                    const auto &linguist = imported->binding()->target();
                    const auto *languageSpec = dynamic_cast<const wolf::LinguistSpec *>(&linguist);
                    if (!languageSpec) {
                        return std::unexpected{inference::ResourceError("The singer language binding is not a Wolf linguist declaration for " + language.handle)};
                    }
                    const auto g2p = linguist.findImport("linguist/g2p");
                    const bool hasG2p = g2p && g2p->binding();
                    pronunciation = pronunciation && hasG2p;
                    commonDepth = std::min(commonDepth, status.maxDepth);
                    completeLanguagePipeline = completeLanguagePipeline && hasG2p && status.maxDepth == Depth::Onsets;
                    record.languages.emplace(language.handle, LanguageRecord{inference::languageMetadata(*languageSpec), status.maxDepth});
                    capabilities[language.handle]["maxDepth"] = depthName(status.maxDepth);
                }
            }
            if (record.languages.empty()) {
                commonDepth = Depth::Pronunciation;
                pronunciation = false;
                completeLanguagePipeline = false;
            } else {
                if (!record.languages.contains(singer.defaultLanguage())) {
                    log.stdcWarning("Singer %1 has an unavailable declared default language %2", record.info.id, singer.defaultLanguage());
                }
                record.info.defaultLanguage = singer.defaultLanguage();
            }
            group.pronunciationMode = pronunciation ? arch::PronunciationMode::Full : arch::PronunciationMode::Skip;
            group.phonemeMode = commonDepth == Depth::Onsets ? arch::PhonemeMode::Full : commonDepth == Depth::Phonemes ? arch::PhonemeMode::TokenOnly : arch::PhonemeMode::Skip;

            const auto *acousticSchema = dynamic_cast<const Ac::AcousticSchema *>(acoustic->spec->exports());
            const auto *acousticConfiguration = dynamic_cast<const Ac::AcousticConfiguration *>(acoustic->spec->configuration());
            const auto *vocoderConfiguration = dynamic_cast<const Vo::VocoderConfiguration *>(vocoder->spec->configuration());
            const auto supported = [&](std::string_view parameter) {
                const ds::ParamTag tag{std::string(parameter)};
                return acousticConfiguration->parameters.contains(tag) && (acousticSchema->varianceControls.contains(tag) || acousticSchema->transitionControls.contains(tag));
            };
            bool expressiveness = false;
            if (pitch->spec) {
                const auto *schema = dynamic_cast<const Pi::PitchSchema *>(pitch->spec->exports());
                const auto *config = dynamic_cast<const Pi::PitchConfiguration *>(pitch->spec->configuration());
                expressiveness = schema->allowExpressiveness && config->useExpressiveness;
                group.parameterPipeline.emplace("pitch", arch::IndirectParameter{expressiveness ? std::vector<std::string>{"expressiveness"} : std::vector<std::string>{}});
            } else {
                group.parameterPipeline.emplace("pitch", arch::DirectParameter{});
            }
            group.audioDependencies.push_back("pitch");
            if (expressiveness) {
                group.parameterPipeline.emplace("expressiveness", arch::DirectParameter{});
            }
            std::map<std::string, srt::JsonValue> effectiveOptions;
            for (const auto role : {"singer/duration", "singer/pitch", "singer/acoustic", "singer/vocoder"}) {
                effectiveOptions.emplace(role, srt::JsonObject{});
            }
            if (variance->spec) {
                const auto *schema = dynamic_cast<const Va::VarianceSchema *>(variance->spec->exports());
                const auto *options = dynamic_cast<const Va::VarianceImportOptions *>(variance->options);
                if (options->predictions.empty()) {
                    return std::unexpected{inference::ResourceError("The variance import must select a nonempty prediction subset")};
                }
                srt::JsonArray predictions;
                for (const auto &prediction : schema->predictions) {
                    if (options->predictions.contains(prediction)) {
                        predictions.push_back(prediction.name());
                        if (supported(prediction.name())) {
                            record.variancePredictions.push_back(prediction.name());
                        }
                    }
                }
                effectiveOptions.emplace("singer/variance", srt::JsonObject{{"predictions", std::move(predictions)}});
            }
            for (const char *parameter : {"energy", "breathiness", "voicing", "tension", "mouth_opening", "gender", "velocity"}) {
                if (!supported(parameter)) {
                    continue;
                }
                if (std::string_view(parameter) != "gender" && std::string_view(parameter) != "velocity" && std::ranges::find(record.variancePredictions, parameter) != record.variancePredictions.end()) {
                    std::vector<std::string> dependencies{"pitch"};
                    if (vocoderConfiguration->pitchControllable) {
                        dependencies.push_back("tone_shift");
                    }
                    group.parameterPipeline.emplace(parameter, arch::IndirectParameter{std::move(dependencies)});
                } else {
                    group.parameterPipeline.emplace(parameter, arch::DirectParameter{});
                }
                group.audioDependencies.push_back(parameter);
            }
            if (vocoderConfiguration->pitchControllable) {
                group.parameterPipeline.emplace("tone_shift", arch::DirectParameter{});
                group.audioDependencies.push_back("tone_shift");
            }
            record.technicallyMixable = completeLanguagePipeline && duration->spec && pitch->spec && variance->spec && duration->usesSpeakers && variance->usesSpeakers && acoustic->usesSpeakers && !record.speakers.empty();
            group.mixable = record.technicallyMixable && record.permitsMixing;

            // Effective imports include every language and every stage, with exact
            // resolved versions and recursive resources; public speaker selection
            // and host permissions do not alter this pipeline identity.
            std::map<std::string, std::string> roleNames;
            for (const auto &[handle, role] : singer.languages()) {
                roleNames.emplace(role, "language/" + handle);
                effectiveOptions.emplace(role, srt::JsonObject{});
            }
            // The pinned provider retains dict but does not consume it. Wolf's
            // effective language resources already occur in the binding graph.
            const auto description = srt::JsonValue(srt::JsonObject{{"pipeline", inference::describeImports(singer, effectiveOptions, roleNames)}, {"reservedPhonemes", srt::JsonArray{}}});
            auto identity = description.toObject();
            srt::JsonArray reserved;
            const std::set<std::string> reservedTokens(singer.reservedPhonemes().begin(), singer.reservedPhonemes().end());
            for (const auto &token : reservedTokens) {
                reserved.push_back(token);
            }
            identity["reservedPhonemes"] = std::move(reserved);
            srt::JsonObject languages;
            for (const auto &[handle, language] : record.languages) {
                languages.emplace(handle, depthName(language.depth));
            }
            identity["languages"] = std::move(languages);
            auto digest = inference::fingerprint(srt::JsonValue(std::move(identity)).toJson());
            if (!digest) {
                return std::unexpected{std::move(digest.error())};
            }
            group.id = "g1." + *digest;
            record.info.group = group.id;
            record.info.archSpecificInfo = Json::Value(Json::objectValue);
            record.info.archSpecificInfo["speakers"] = Json::Value(Json::arrayValue);
            for (const auto &speaker : record.speakers) {
                record.info.archSpecificInfo["speakers"].append(speaker);
            }
            record.info.archSpecificInfo["hasDuration"] = duration->spec != nullptr;
            record.info.archSpecificInfo["languageCapabilities"] = std::move(capabilities);
            record.info.defaultExtra = Json::Value(Json::objectValue);
            record.info.defaultExtra["speaker"] = record.speakers.empty() ? std::string{} : record.speakers.front();
            return std::make_pair(std::move(record), std::move(group));
        }
    }

    std::vector<std::string> commonSpeakers(const std::map<std::string, StageRecord> &stages) {
        std::optional<std::vector<std::string>> result;
        for (const auto &[role, stage] : stages) {
            if (!stage.usesSpeakers) {
                continue;
            }
            std::vector<std::string> available;
            if (stage.speakerMapping.empty()) {
                for (const auto &speaker : stage.speakers) {
                    if (stage.embeddedSpeakers.contains(speaker) && std::ranges::find(available, speaker) == available.end()) {
                        available.push_back(speaker);
                    }
                }
            } else {
                for (const auto &[logical, target] : stage.speakerMapping) {
                    if (stage.embeddedSpeakers.contains(target) && std::ranges::find(stage.speakers, target) != stage.speakers.end()) {
                        available.push_back(logical);
                    }
                }
            }
            if (!result) {
                result = std::move(available);
            } else {
                std::erase_if(*result, [&](const auto &speaker) { return std::ranges::find(available, speaker) == available.end(); });
            }
        }
        return result.value_or(std::vector<std::string>{});
    }

    arch::CatalogResult<arch::SingerInfo> singerInfo(const SingerRecord &record, const arch::CatalogOptions &display) {
        auto result = record.info;
        auto name = inference::localize(record.name, display.acceptLanguage, result.id);
        if (!name) {
            return fail<arch::CatalogErrors>(arch::OperationError("Cannot localize singer name", name.error().clone()));
        }
        result.name = std::move(*name);
        for (const auto &[handle, language] : record.languages) {
            auto title = inference::localize(language.metadata.name, display.acceptLanguage, handle);
            if (!title) {
                return fail<arch::CatalogErrors>(arch::OperationError("Cannot localize language name", title.error().clone()));
            }
            result.languages.emplace(handle, arch::SingerLanguageInfo{std::move(*title), language.metadata.defaultLyric});
        }
        return result;
    }

    std::expected<std::shared_ptr<const SingerCatalog>, inference::ResourceError> SingerCatalog::discover(std::shared_ptr<inference::Environment> environment) {
        if (!environment) {
            return std::unexpected{inference::ResourceError("Singer discovery requires an inference environment")};
        }
        try {
            auto catalog = std::shared_ptr<SingerCatalog>(new SingerCatalog(std::move(environment)));
            for (const auto &package : catalog->environment_->packages()) {
                for (const auto *contribution : package.contributions("singer")) {
                    if (contribution->interface() != Ds::API_INTERFACE || contribution->variant() != Ds::API_VARIANT || contribution->level() != Ds::API_LEVEL) {
                        continue;
                    }
                    const auto *singer = dynamic_cast<const srt::SingerSpec *>(contribution);
                    if (!singer) {
                        log.stdcWarning("Skipping an invalid singer category declaration in %1", package.id());
                        continue;
                    }
                    auto found = readSinger(*singer, *catalog->environment_);
                    if (!found) {
                        log.stdcWarning("Skipping DiffSinger singer %1 in %2@%3: %4", singer->locator().contributionId(), package.id(), package.version().toString(), found.error().formatChain());
                        continue;
                    }
                    auto [record, group] = std::move(*found);
                    const auto [existing, inserted] = catalog->groups_.emplace(group.id, group);
                    if (!inserted) {
                        existing->second.mixable = existing->second.mixable && group.mixable;
                    }
                    const auto id = record.info.id;
                    catalog->singers_.emplace(id, std::move(record));
                }
            }
            log.stdcInfo("Discovered %1 DiffSinger singers in %2 groups", catalog->singers_.size(), catalog->groups_.size());
            return std::shared_ptr<const SingerCatalog>(std::move(catalog));
        } catch (const std::exception &error) {
            return std::unexpected{inference::ResourceError("Cannot read the loaded singer declarations", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"synthrt/Wolf/dsinfer", "discover singers", "exception"}))};
        }
    }

    const SingerRecord *SingerCatalog::find(std::string_view id) const {
        const auto found = singers_.find(std::string(id));
        return found == singers_.end() ? nullptr : &found->second;
    }
}
