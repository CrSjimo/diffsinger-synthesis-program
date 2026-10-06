#include "Controllers.h"

#include <stdcorelib/support/logging.h>

#include <dsspapiv1/ArchitectureDispatch.h>
#include <dsspapiv1/ControllerSupport.h>

namespace dssp::api_v1 {
    namespace {
        constexpr dto::ApiVersion apiVersion{1.0, 0.0};
        stdc::LogCategory applicationLog("dssp.api_v1.ApplicationController");
        stdc::LogCategory catalogLog("dssp.api_v1.CatalogController");
        stdc::LogCategory synthesisLog("dssp.api_v1.SynthesisController");
        stdc::LogCategory extractionLog("dssp.api_v1.ExtractionController");
    }

    void ApplicationController::info(const drogon::HttpRequestPtr &, http::Callback &&cb) const {
        applicationLog.stdcDebug("Handling info");
        const dto::ApplicationInfoResponse info{{apiVersion}};
        cb(http::jsonResponse(toJson(info)));
    }
    void CatalogController::architectures(const drogon::HttpRequestPtr &req, http::Callback &&cb) const {
        catalogLog.stdcDebug("Listing architectures");
        std::vector<dto::ArchitectureMetadata> result;
        for (const auto &implementation : architectures_->entries()) {
            auto metadata = implementation->metadata(detail::catalogOptions(*req));
            if (!metadata) {
                cb(architectureProblemResponse(metadata.error()));
                return;
            }
            result.push_back(toDto(std::move(*metadata)));
        }
        cb(http::jsonResponse(toJson(result)));
    }
    void CatalogController::architecture(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id) const {
        detail::catalog(*architectures_, cb, id, detail::catalogOptions(*req), &arch::Architecture::metadata);
    }
    void CatalogController::singers(const drogon::HttpRequestPtr &req, http::Callback &&cb) const {
        catalogLog.stdcDebug("Listing singers across architectures");
        std::vector<dto::SingerInfo> result;
        for (const auto &implementation : architectures_->entries()) {
            auto singers = implementation->singers(detail::catalogOptions(*req));
            if (!singers) {
                cb(architectureProblemResponse(singers.error()));
                return;
            }
            for (auto &singer : *singers) {
                result.push_back(toDto(std::move(singer)));
            }
        }
        cb(http::jsonResponse(toJson(result)));
    }
    void CatalogController::archSingers(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id) const {
        detail::catalog(*architectures_, cb, id, detail::catalogOptions(*req), &arch::Architecture::singers);
    }
    void CatalogController::singer(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id, std::string singer) const {
        detail::catalog(*architectures_, cb, id, arch::ResourceQuery{std::move(singer), detail::catalogOptions(*req)}, &arch::Architecture::singer);
    }
    void CatalogController::avatar(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id, std::string singer) const {
        detail::catalogMedia(*architectures_, req, cb, id, std::move(singer), &arch::Architecture::avatar, [](std::string url) { return dto::SingerAvatarResponse{std::move(url)}; });
    }
    void CatalogController::background(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id, std::string singer) const {
        detail::catalogMedia(*architectures_, req, cb, id, std::move(singer), &arch::Architecture::background, [](std::string url) { return dto::SingerBackgroundResponse{std::move(url)}; });
    }
    void CatalogController::demoAudio(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id, std::string singer) const {
        detail::catalogMedia(*architectures_, req, cb, id, std::move(singer), &arch::Architecture::demoAudio);
    }
    void CatalogController::groups(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id) const {
        detail::catalog(*architectures_, cb, id, detail::catalogOptions(*req), &arch::Architecture::groups);
    }
    void CatalogController::group(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id, std::string group) const {
        detail::catalog(*architectures_, cb, id, arch::ResourceQuery{std::move(group), detail::catalogOptions(*req)}, &arch::Architecture::group);
    }
    void CatalogController::groupSingers(const drogon::HttpRequestPtr &req, http::Callback &&cb, std::string id, std::string group) const {
        detail::catalog(*architectures_, cb, id, arch::ResourceQuery{std::move(group), detail::catalogOptions(*req)}, &arch::Architecture::groupSingers);
    }

    void SynthesisController::envTag(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        synthesisLog.stdcDebug("Handling envTag");
        auto input = request->as<Decoded<dto::EnvTagRequest>>();
        if (!input) {
            callback(http::problemResponse(input.error()));
            return;
        }
        const auto selected = architectures_->find(input->context.arch);
        if (!selected) {
            callback(detail::unknownProcessArchitecture(input->context.arch));
            return;
        }
        detail::respond(selected->envTag(toArchitecture(std::move(*input))), callback, [](std::string tag) { return dto::EnvTagResponse{std::move(tag)}; });
    }
    void SynthesisController::pronunciation(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        synthesisLog.stdcDebug("Handling pronunciation");
        detail::process<dto::PronunciationRequest, dto::PronunciationResponse, arch::PronunciationOutput>(request, std::move(callback), lifetime_, *architectures_, &arch::Architecture::pronunciation);
    }
    void SynthesisController::phoneme(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        synthesisLog.stdcDebug("Handling phoneme");
        detail::process<dto::PhonemeRequest, dto::PhonemeResponse, arch::PhonemeOutput>(request, std::move(callback), lifetime_, *architectures_, &arch::Architecture::phoneme);
    }
    void SynthesisController::duration(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        synthesisLog.stdcDebug("Handling duration");
        detail::process<dto::DurationRequest, dto::DurationResponse, arch::DurationOutput>(request, std::move(callback), lifetime_, *architectures_, &arch::Architecture::duration);
    }
    void SynthesisController::parameter(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        synthesisLog.stdcDebug("Handling parameter");
        detail::process<dto::ParameterRequest, dto::ParameterResponse, arch::ParameterOutput>(request, std::move(callback), lifetime_, *architectures_, &arch::Architecture::parameter);
    }
    void SynthesisController::audio(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        synthesisLog.stdcDebug("Handling audio");
        detail::process<dto::AudioRequest, dto::AudioResponse, arch::AudioOutput>(request, std::move(callback), lifetime_, *architectures_, &arch::Architecture::audio);
    }

    void ExtractionController::extractors(const drogon::HttpRequestPtr &req, http::Callback &&cb) const {
        detail::placeholder<dto::ExtractorList>(req, cb, lifetime_);
    }
    void ExtractionController::noteExtractor(const drogon::HttpRequestPtr &request, http::Callback &&callback, std::string) const {
        detail::placeholder<std::optional<dto::ExtractorInfo>>(request, callback, lifetime_);
    }
    void ExtractionController::tempoExtractor(const drogon::HttpRequestPtr &request, http::Callback &&callback, std::string) const {
        detail::placeholder<std::optional<dto::ExtractorInfo>>(request, callback, lifetime_);
    }
    void ExtractionController::pitchExtractor(const drogon::HttpRequestPtr &request, http::Callback &&callback, std::string) const {
        detail::placeholder<std::optional<dto::ExtractorInfo>>(request, callback, lifetime_);
    }
    void ExtractionController::separationExtractor(const drogon::HttpRequestPtr &request, http::Callback &&callback, std::string) const {
        detail::placeholder<std::optional<dto::ExtractorInfo>>(request, callback, lifetime_);
    }

    void ExtractionController::note(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        extractionLog.stdcDebug("Handling note");
        detail::processPlaceholder<dto::ExtractionRequest, dto::NoteExtractionResponse>(request, callback, lifetime_);
    }
    void ExtractionController::tempo(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        extractionLog.stdcDebug("Handling tempo");
        detail::processPlaceholder<dto::ExtractionRequest, dto::TempoExtractionResponse>(request, callback, lifetime_);
    }
    void ExtractionController::pitch(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        extractionLog.stdcDebug("Handling pitch");
        detail::processPlaceholder<dto::ExtractionRequest, dto::PitchExtractionResponse>(request, callback, lifetime_);
    }
    void ExtractionController::separation(const drogon::HttpRequestPtr &request, http::Callback &&callback) const {
        extractionLog.stdcDebug("Handling separation");
        detail::processPlaceholder<dto::ExtractionRequest, dto::SeparationExtractionResponse>(request, callback, lifetime_);
    }

    void registerControllers(drogon::HttpAppFramework &app, const std::shared_ptr<http::RequestLifetimeRegistry> &lifetime, const std::shared_ptr<const arch::ArchitectureCatalog> &architectures) {
        app.registerController(std::make_shared<ApplicationController>());
        app.registerController(std::make_shared<CatalogController>(architectures));
        app.registerController(std::make_shared<SynthesisController>(lifetime, architectures));
        app.registerController(std::make_shared<ExtractionController>(lifetime));
    }

}
