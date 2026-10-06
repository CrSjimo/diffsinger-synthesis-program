#include "DiffSinger.h"

#include <algorithm>
#include <exception>

#include <stdcorelib/path.h>
#include <stdcorelib/str.h>
#include <stdcorelib/support/logging.h>

#include <dssparchdiffsinger/Factory.h>
#include <dssparchdiffsinger/Metadata.h>
#include <dsspinference/Display.h>
#include <dsspmedia/Resources.h>

namespace dssp::arch_diffsinger {
    namespace {
        stdc::LogCategory log("dssp.arch_diffsinger.DiffSinger");
        template <class T, class Function>
        arch::CatalogResult<T> catalogueCall(Function &&function) {
            try {
                return function();
            } catch (const std::exception &error) {
                return fail<arch::CatalogErrors>(arch::OperationError("Cannot read singer metadata", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"metadata libraries", "read singer metadata", "exception"})));
            }
        }
        arch::CatalogResult<std::string> mediaUrl(const media::Resources &resources, const SingerRecord &record, const srt::DisplayText &path, const arch::SingerMediaQuery &query, media::Kind kind) {
            const auto selected = inference::localize(path, query.singer.display.acceptLanguage);
            if (!selected) {
                return fail<arch::CatalogErrors>(arch::OperationError("Cannot localize singer media path", selected.error().clone()));
            }
            if (selected->empty()) {
                return std::string{};
            }
            auto file = stdc::path::from_utf8(*selected);
            if (file.is_relative()) {
                file = record.package.path() / file;
            }
            auto resource = resources.read(file, kind);
            if (!resource) {
                return fail<arch::CatalogErrors>(arch::OperationError("Cannot read the declared singer media resource", resource.error().clone()));
            }
            if (!query.media.acceptableSchemes.empty() && std::ranges::none_of(query.media.acceptableSchemes, [](const auto &scheme) { return stdc::str::equals_insensitive(scheme, "data"); })) {
                return fail<arch::CatalogErrors>(arch::InvalidInputError("acceptableSchemes does not accept the resource scheme data"));
            }
            if (!query.media.acceptableFormats.empty() && std::ranges::none_of(query.media.acceptableFormats, [&](const auto &format) { return stdc::str::equals_insensitive(format, resource->mime); })) {
                return fail<arch::CatalogErrors>(arch::InvalidInputError("acceptableFormats does not accept the resource format " + resource->mime));
            }
            return std::move(resource->dataUrl);
        }
        template <class Output>
        void completeEmpty(std::stop_token stop, arch::Completion<Output> completion) {
            if (stop.stop_requested()) {
                completion(fail<arch::SynthesisErrors>(arch::CancelledError("Synthesis request was cancelled")));
                return;
            }
            completion(Output{});
        }
    }

    std::string_view DiffSinger::id() const noexcept {
        return Id;
    }
    arch::CatalogResult<arch::ArchitectureMetadata> DiffSinger::metadata(const arch::CatalogOptions &options) const {
        return catalogueCall<arch::ArchitectureMetadata>([&] { return architectureMetadata(options, *translations_); });
    }
    arch::CatalogResult<std::vector<arch::SingerInfo>> DiffSinger::singers(const arch::CatalogOptions &options) const {
        return catalogueCall<std::vector<arch::SingerInfo>>([&]() -> arch::CatalogResult<std::vector<arch::SingerInfo>> {
            std::vector<arch::SingerInfo> result;
            result.reserve(catalog_->singers().size());
            for (const auto &[id, singer] : catalog_->singers()) {
                auto info = singerInfo(singer, options);
                if (!info) {
                    return std::unexpected{std::move(info.error())};
                }
                result.push_back(std::move(*info));
            }
            return result;
        });
    }
    arch::CatalogResult<std::optional<arch::SingerInfo>> DiffSinger::singer(const arch::ResourceQuery &query) const {
        return catalogueCall<std::optional<arch::SingerInfo>>([&]() -> arch::CatalogResult<std::optional<arch::SingerInfo>> {
            const auto *record = catalog_->find(query.id);
            if (!record) {
                return fail<arch::CatalogErrors>(arch::ResourceNotFoundError("Singer does not exist: " + query.id));
            }
            auto info = singerInfo(*record, query.display);
            if (!info) {
                return std::unexpected{std::move(info.error())};
            }
            return std::optional{std::move(*info)};
        });
    }
    arch::CatalogResult<std::string> DiffSinger::avatar(const arch::SingerMediaQuery &query) const {
        return catalogueCall<std::string>([&]() -> arch::CatalogResult<std::string> {
            const auto *record = catalog_->find(query.singer.id);
            if (!record) {
                return fail<arch::CatalogErrors>(arch::ResourceNotFoundError("Singer does not exist: " + query.singer.id));
            }
            return mediaUrl(*media_, *record, record->avatar, query, media::Kind::Image);
        });
    }
    arch::CatalogResult<std::string> DiffSinger::background(const arch::SingerMediaQuery &query) const {
        return catalogueCall<std::string>([&]() -> arch::CatalogResult<std::string> {
            const auto *record = catalog_->find(query.singer.id);
            if (!record) {
                return fail<arch::CatalogErrors>(arch::ResourceNotFoundError("Singer does not exist: " + query.singer.id));
            }
            return mediaUrl(*media_, *record, record->background, query, media::Kind::Image);
        });
    }
    arch::CatalogResult<std::vector<arch::SingerDemoAudio>> DiffSinger::demoAudio(const arch::SingerMediaQuery &query) const {
        return catalogueCall<std::vector<arch::SingerDemoAudio>>([&]() -> arch::CatalogResult<std::vector<arch::SingerDemoAudio>> {
            const auto *record = catalog_->find(query.singer.id);
            if (!record) {
                return fail<arch::CatalogErrors>(arch::ResourceNotFoundError("Singer does not exist: " + query.singer.id));
            }
            auto url = mediaUrl(*media_, *record, record->demoAudio, query, media::Kind::Audio);
            if (!url) {
                return std::unexpected{std::move(url.error())};
            }
            std::vector<arch::SingerDemoAudio> result;
            if (!url->empty()) {
                auto name = inference::localize(record->name, query.singer.display.acceptLanguage, record->info.id);
                if (!name) {
                    return fail<arch::CatalogErrors>(arch::OperationError("Cannot localize demo audio name", name.error().clone()));
                }
                result.push_back({std::move(*name), std::move(*url)});
            }
            return result;
        });
    }
    arch::CatalogResult<std::vector<arch::GroupMetadata>> DiffSinger::groups(const arch::CatalogOptions &) const {
        std::vector<arch::GroupMetadata> result;
        result.reserve(catalog_->groups().size());
        for (const auto &[id, group] : catalog_->groups()) {
            result.push_back(group);
        }
        return result;
    }
    arch::CatalogResult<std::optional<arch::GroupMetadata>> DiffSinger::group(const arch::ResourceQuery &query) const {
        const auto found = catalog_->groups().find(query.id);
        if (found == catalog_->groups().end()) {
            return fail<arch::CatalogErrors>(arch::ResourceNotFoundError("Group does not exist: " + query.id));
        }
        return std::optional{found->second};
    }
    arch::CatalogResult<std::vector<arch::SingerInfo>> DiffSinger::groupSingers(const arch::ResourceQuery &query) const {
        return catalogueCall<std::vector<arch::SingerInfo>>([&]() -> arch::CatalogResult<std::vector<arch::SingerInfo>> {
            if (!catalog_->groups().contains(query.id)) {
                return fail<arch::CatalogErrors>(arch::ResourceNotFoundError("Group does not exist: " + query.id));
            }
            std::vector<arch::SingerInfo> result;
            for (const auto &[id, singer] : catalog_->singers()) {
                if (singer.info.group == query.id) {
                    auto info = singerInfo(singer, query.display);
                    if (!info) {
                        return std::unexpected{std::move(info.error())};
                    }
                    result.push_back(std::move(*info));
                }
            }
            return result;
        });
    }
    std::expected<std::string, arch::EnvironmentErrors> DiffSinger::envTag(arch::MultiSingerContext) const {
        return std::string{};
    }
    void DiffSinger::pronunciation(arch::PronunciationInput, std::stop_token stop, arch::Completion<arch::PronunciationOutput> cb) const {
        log.stdcDebug("Handling pronunciation");
        completeEmpty(stop, std::move(cb));
    }
    void DiffSinger::phoneme(arch::PhonemeInput, std::stop_token stop, arch::Completion<arch::PhonemeOutput> cb) const {
        log.stdcDebug("Handling phoneme");
        completeEmpty(stop, std::move(cb));
    }
    void DiffSinger::duration(arch::DurationInput, std::stop_token stop, arch::Completion<arch::DurationOutput> cb) const {
        log.stdcDebug("Handling duration");
        completeEmpty(stop, std::move(cb));
    }
    void DiffSinger::parameter(arch::ParameterInput, std::stop_token stop, arch::Completion<arch::ParameterOutput> cb) const {
        log.stdcDebug("Handling parameter");
        completeEmpty(stop, std::move(cb));
    }
    void DiffSinger::audio(arch::AudioInput, std::stop_token stop, arch::Completion<arch::AudioOutput> cb) const {
        log.stdcDebug("Handling audio");
        completeEmpty(stop, std::move(cb));
    }

}
