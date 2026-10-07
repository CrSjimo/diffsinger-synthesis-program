#ifndef DSSP_API_V1_CONTROLLERS_CONTROLLERS_H
#define DSSP_API_V1_CONTROLLERS_CONTROLLERS_H

#include <memory>
#include <utility>

#include <drogon/HttpController.h>

#include <dssparch/ArchitectureCatalog.h>
#include <dssphttp/RequestLifetime.h>
#include <dssphttp/Response.h>

namespace dssp::api_v1 {

    class ControllerSupport {
    public:
        explicit ControllerSupport(std::shared_ptr<http::RequestLifetimeRegistry> lifetime) : lifetime_(std::move(lifetime)) {}

    protected:
        std::shared_ptr<http::RequestLifetimeRegistry> lifetime_;
    };

    class ApplicationController final : public drogon::HttpController<ApplicationController, false> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(ApplicationController::info, "/v1/info", drogon::Get);
        METHOD_LIST_END
        void info(const drogon::HttpRequestPtr &, http::Callback &&) const;
    };

    class CatalogController final : public drogon::HttpController<CatalogController, false> {
    public:
        explicit CatalogController(std::shared_ptr<const arch::ArchitectureCatalog> architectures) : architectures_(std::move(architectures)) {}
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(CatalogController::architectures, "/v1/arch", drogon::Get);
            ADD_METHOD_TO(CatalogController::architecture, "/v1/arch/{1}", drogon::Get);
            ADD_METHOD_TO(CatalogController::singers, "/v1/singer", drogon::Get);
            ADD_METHOD_TO(CatalogController::archSingers, "/v1/arch/{1}/singer", drogon::Get);
            // Drogon decodes the path before routing, including %2F inside a reference.
            // Capture package:category/contribution as one ID. Its final contribution
            // segment also keeps media routes distinct when a singer is named "avatar".
            ADD_METHOD_VIA_REGEX(CatalogController::singer, R"(/v1/arch/([^/]+)/singer/([^:]+:[^/]+/[^/]+))", drogon::Get);
            ADD_METHOD_VIA_REGEX(CatalogController::avatar, R"(/v1/arch/([^/]+)/singer/([^:]+:[^/]+/[^/]+)/avatar)", drogon::Get);
            ADD_METHOD_VIA_REGEX(CatalogController::background, R"(/v1/arch/([^/]+)/singer/([^:]+:[^/]+/[^/]+)/background)", drogon::Get);
            ADD_METHOD_VIA_REGEX(CatalogController::demoAudio, R"(/v1/arch/([^/]+)/singer/([^:]+:[^/]+/[^/]+)/demo-audio)", drogon::Get);
            ADD_METHOD_TO(CatalogController::groups, "/v1/arch/{1}/group", drogon::Get);
            ADD_METHOD_TO(CatalogController::group, "/v1/arch/{1}/group/{2}", drogon::Get);
            ADD_METHOD_TO(CatalogController::groupSingers, "/v1/arch/{1}/group/{2}/singer", drogon::Get);
        METHOD_LIST_END
        void architectures(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void architecture(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch) const;
        void singers(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void archSingers(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch) const;
        void singer(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch, std::string singer) const;
        void avatar(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch, std::string singer) const;
        void background(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch, std::string singer) const;
        void demoAudio(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch, std::string singer) const;
        void groups(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch) const;
        void group(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch, std::string group) const;
        void groupSingers(const drogon::HttpRequestPtr &, http::Callback &&, std::string arch, std::string group) const;

    private:
        const std::shared_ptr<const arch::ArchitectureCatalog> architectures_;
    };

    class SynthesisController final : public drogon::HttpController<SynthesisController, false>, public ControllerSupport {
    public:
        SynthesisController(std::shared_ptr<http::RequestLifetimeRegistry> lifetime, std::shared_ptr<const arch::ArchitectureCatalog> architectures) : ControllerSupport(std::move(lifetime)), architectures_(std::move(architectures)) {}
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(SynthesisController::envTag, "/v1/synth/env-tag", drogon::Post);
            ADD_METHOD_TO(SynthesisController::pronunciation, "/v1/synth/pronunciation", drogon::Post);
            ADD_METHOD_TO(SynthesisController::phoneme, "/v1/synth/phoneme", drogon::Post);
            ADD_METHOD_TO(SynthesisController::duration, "/v1/synth/duration", drogon::Post);
            ADD_METHOD_TO(SynthesisController::parameter, "/v1/synth/parameter", drogon::Post);
            ADD_METHOD_TO(SynthesisController::audio, "/v1/synth/audio", drogon::Post);
        METHOD_LIST_END
        void envTag(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void pronunciation(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void phoneme(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void duration(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void parameter(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void audio(const drogon::HttpRequestPtr &, http::Callback &&) const;

    private:
        const std::shared_ptr<const arch::ArchitectureCatalog> architectures_;
    };

    class ExtractionController final : public drogon::HttpController<ExtractionController, false>, public ControllerSupport {
    public:
        using ControllerSupport::ControllerSupport;
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(ExtractionController::extractors, "/v1/extractor", drogon::Get);
            ADD_METHOD_TO(ExtractionController::noteExtractor, "/v1/extractor/note/{1}", drogon::Get);
            ADD_METHOD_TO(ExtractionController::tempoExtractor, "/v1/extractor/tempo/{1}", drogon::Get);
            ADD_METHOD_TO(ExtractionController::pitchExtractor, "/v1/extractor/pitch/{1}", drogon::Get);
            ADD_METHOD_TO(ExtractionController::separationExtractor, "/v1/extractor/separation/{1}", drogon::Get);
            ADD_METHOD_TO(ExtractionController::note, "/v1/extract/note", drogon::Post);
            ADD_METHOD_TO(ExtractionController::tempo, "/v1/extract/tempo", drogon::Post);
            ADD_METHOD_TO(ExtractionController::pitch, "/v1/extract/pitch", drogon::Post);
            ADD_METHOD_TO(ExtractionController::separation, "/v1/extract/separation", drogon::Post);
        METHOD_LIST_END
        void extractors(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void noteExtractor(const drogon::HttpRequestPtr &, http::Callback &&, std::string id) const;
        void tempoExtractor(const drogon::HttpRequestPtr &, http::Callback &&, std::string id) const;
        void pitchExtractor(const drogon::HttpRequestPtr &, http::Callback &&, std::string id) const;
        void separationExtractor(const drogon::HttpRequestPtr &, http::Callback &&, std::string id) const;
        void note(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void tempo(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void pitch(const drogon::HttpRequestPtr &, http::Callback &&) const;
        void separation(const drogon::HttpRequestPtr &, http::Callback &&) const;
    };

    void registerControllers(drogon::HttpAppFramework &app, const std::shared_ptr<http::RequestLifetimeRegistry> &lifetime, const std::shared_ptr<const arch::ArchitectureCatalog> &architectures);

}

#endif // DSSP_API_V1_CONTROLLERS_CONTROLLERS_H
