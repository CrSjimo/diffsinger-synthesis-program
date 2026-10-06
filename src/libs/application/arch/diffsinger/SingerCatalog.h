#ifndef DSSP_ARCHDIFFSINGER_SINGERCATALOG_H
#define DSSP_ARCHDIFFSINGER_SINGERCATALOG_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <synthrt/Core/ContribSpec.h>
#include <synthrt/SVS/SingerContrib.h>
#include <wolf/Session/LinguistSession.h>

#include <dssparch/Errors.h>
#include <dssparch/Types.h>
#include <dsspinference/Environment.h>
#include <dsspinference/LanguageMetadata.h>

namespace dssp::arch_diffsinger {

    struct StageRecord {
        const srt::ContribSpec *spec = nullptr;
        const srt::ContribImportOptions *options = nullptr;
        bool usesSpeakers = false;
        std::vector<std::string> speakers;
        std::set<std::string> embeddedSpeakers;
        std::map<std::string, std::string> speakerMapping;
    };
    struct LanguageRecord {
        inference::LanguageMetadata metadata;
        wolf::Api::Linguist::L1::Depth depth = wolf::Api::Linguist::L1::Depth::Pronunciation;
    };
    struct SingerRecord {
        srt::PackageHandle package;
        const srt::SingerSpec *spec = nullptr;
        wolf::SingerRef ref;
        arch::SingerInfo info;
        srt::DisplayText name;
        srt::DisplayText avatar;
        srt::DisplayText background;
        srt::DisplayText demoAudio;
        std::map<std::string, StageRecord> stages;
        std::map<std::string, LanguageRecord> languages;
        std::vector<std::string> speakers;
        // Keep the model's exported order after applying the singer's import subset.
        std::vector<std::string> variancePredictions;
        bool technicallyMixable = false;
        bool permitsMixing = true;
    };

    // Only declaration data is retained. No pipeline/executive is created here.
    class SingerCatalog final {
    public:
        static std::expected<std::shared_ptr<const SingerCatalog>, inference::ResourceError> discover(std::shared_ptr<inference::Environment>);
        const std::map<std::string, SingerRecord> &singers() const noexcept { return singers_; }
        const std::map<std::string, arch::GroupMetadata> &groups() const noexcept { return groups_; }
        const SingerRecord *find(std::string_view id) const;
        const std::shared_ptr<inference::Environment> &environment() const noexcept { return environment_; }

    private:
        explicit SingerCatalog(std::shared_ptr<inference::Environment> environment) : environment_(std::move(environment)) {}
        // Retains the unit until all borrowed specs and package handles are gone.
        std::shared_ptr<inference::Environment> environment_;
        std::map<std::string, SingerRecord> singers_;
        std::map<std::string, arch::GroupMetadata> groups_;
    };

    std::vector<std::string> commonSpeakers(const std::map<std::string, StageRecord> &);
    arch::CatalogResult<arch::SingerInfo> singerInfo(const SingerRecord &, const arch::CatalogOptions &);
}

#endif // DSSP_ARCHDIFFSINGER_SINGERCATALOG_H
