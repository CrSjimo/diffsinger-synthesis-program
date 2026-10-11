#ifndef DSSP_EXTRACTION_EXTRACTORCATALOG_H
#define DSSP_EXTRACTION_EXTRACTORCATALOG_H

#include <map>
#include <string_view>
#include <utility>

#include <dsspextraction/OtterTypes.h>
#include <dsspextraction/SeparationTypes.h>
#include <dsspextraction/BeatTypes.h>
#include <dsspextraction/Types.h>
#include <dsspinference/Environment.h>

namespace dssp::extraction {
    struct ExtractorRecord {
        Kind kind = Kind::Note;
        std::string id;
        srt::DisplayText name;
        srt::PackageHandle package;
        srt::InferenceSpec *spec = nullptr;
        int sampleRate = 0;
        int channelCount = 1;
        std::vector<srtsepinfer::Api::Separation::L1::TrackInfo> tracks;
        double maximumDuration = 0;
        double outputInterval = 0;
        inference::InferencePool *pool = nullptr;
        inference::InferenceHandle resource;
    };

    class ExtractorCatalog final {
    public:
        static std::expected<std::shared_ptr<const ExtractorCatalog>, inference::InitializationError> discover(std::shared_ptr<inference::Environment>);
        CatalogResult<ExtractorList> list(std::string_view acceptLanguage) const;
        CatalogResult<ExtractorInfo> metadata(Kind, std::string_view id, std::string_view acceptLanguage) const;
        const ExtractorRecord *find(Kind, std::string_view id) const;
        const std::shared_ptr<inference::Environment> &environment() const noexcept { return environment_; }

    private:
        explicit ExtractorCatalog(std::shared_ptr<inference::Environment> environment) : environment_(std::move(environment)) {}

        std::shared_ptr<inference::Environment> environment_;
        std::map<std::string, ExtractorRecord, std::less<>> records_;
    };
}

#endif // DSSP_EXTRACTION_EXTRACTORCATALOG_H
