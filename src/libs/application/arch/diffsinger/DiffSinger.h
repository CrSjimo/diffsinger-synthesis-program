#ifndef DSSP_ARCH_DIFFSINGER_H
#define DSSP_ARCH_DIFFSINGER_H

#include <dssparch/Architecture.h>
#include <dssparchdiffsinger/SingerCatalog.h>
#include <dssplocalization/Catalog.h>
#include <dsspmedia/Resources.h>
#include <dsspresource/TaskExecutor.h>

namespace dssp::arch_diffsinger {

    class DiffSinger final : public arch::Architecture {
    public:
        DiffSinger(std::shared_ptr<const SingerCatalog> catalog, std::shared_ptr<media::Resources> media, std::shared_ptr<const localization::Catalog> translations, std::shared_ptr<resource::TaskExecutor> tasks) : catalog_(std::move(catalog)), media_(std::move(media)), translations_(std::move(translations)), tasks_(std::move(tasks)) {}
        const SingerCatalog &catalog() const noexcept { return *catalog_; }
        std::string_view id() const noexcept override;
        arch::CatalogResult<arch::ArchitectureMetadata> metadata(const arch::CatalogOptions &) const override;
        arch::CatalogResult<std::vector<arch::SingerInfo>> singers(const arch::CatalogOptions &) const override;
        arch::CatalogResult<std::optional<arch::SingerInfo>> singer(const arch::ResourceQuery &) const override;
        arch::CatalogResult<std::string> avatar(const arch::SingerMediaQuery &) const override;
        arch::CatalogResult<std::string> background(const arch::SingerMediaQuery &) const override;
        arch::CatalogResult<std::vector<arch::SingerDemoAudio>> demoAudio(const arch::SingerMediaQuery &) const override;
        arch::CatalogResult<std::vector<arch::GroupMetadata>> groups(const arch::CatalogOptions &) const override;
        arch::CatalogResult<std::optional<arch::GroupMetadata>> group(const arch::ResourceQuery &) const override;
        arch::CatalogResult<std::vector<arch::SingerInfo>> groupSingers(const arch::ResourceQuery &) const override;
        std::expected<std::string, arch::EnvironmentErrors> envTag(arch::MultiSingerContext) const override;
        void pronunciation(arch::PronunciationInput, std::stop_token, arch::Completion<arch::PronunciationOutput>) const override;
        void phoneme(arch::PhonemeInput, std::stop_token, arch::Completion<arch::PhonemeOutput>) const override;
        void duration(arch::DurationInput, std::stop_token, arch::Completion<arch::DurationOutput>) const override;
        void parameter(arch::ParameterInput, std::stop_token, arch::Completion<arch::ParameterOutput>) const override;
        void audio(arch::AudioInput, std::stop_token, arch::Completion<arch::AudioOutput>) const override;

    private:
        std::shared_ptr<const SingerCatalog> catalog_;
        std::shared_ptr<media::Resources> media_;
        std::shared_ptr<const localization::Catalog> translations_;
        std::shared_ptr<resource::TaskExecutor> tasks_;
    };

}

#endif // DSSP_ARCH_DIFFSINGER_H
