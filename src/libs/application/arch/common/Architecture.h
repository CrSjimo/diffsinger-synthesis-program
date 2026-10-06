#ifndef DSSP_ARCH_ARCHITECTURE_H
#define DSSP_ARCH_ARCHITECTURE_H

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include <dssparch/Errors.h>
#include <dssparch/Types.h>

namespace dssp::arch {

    template <class T>
    using Completion = std::function<void(SynthesisResult<T>)>;

    // Implementations must support concurrent const calls. Each operation owns
    // its inputs and cancellation token; mutable runtime resources need their
    // own synchronization. Do not hold a resource lock while calling completion.
    class Architecture {
    public:
        virtual ~Architecture() = default;
        // The ID is nonempty and remains stable for the instance's lifetime.
        virtual std::string_view id() const noexcept = 0;
        virtual CatalogResult<ArchitectureMetadata> metadata(const CatalogOptions &) const = 0;
        virtual CatalogResult<std::vector<SingerInfo>> singers(const CatalogOptions &) const = 0;
        virtual CatalogResult<std::optional<SingerInfo>> singer(const ResourceQuery &) const = 0;
        virtual CatalogResult<std::string> avatar(const SingerMediaQuery &) const = 0;
        virtual CatalogResult<std::string> background(const SingerMediaQuery &) const = 0;
        virtual CatalogResult<std::vector<SingerDemoAudio>> demoAudio(const SingerMediaQuery &) const = 0;
        virtual CatalogResult<std::vector<GroupMetadata>> groups(const CatalogOptions &) const = 0;
        virtual CatalogResult<std::optional<GroupMetadata>> group(const ResourceQuery &) const = 0;
        virtual CatalogResult<std::vector<SingerInfo>> groupSingers(const ResourceQuery &) const = 0;
        virtual std::expected<std::string, EnvironmentErrors> envTag(MultiSingerContext) const = 0;

        // Completion may be called inline or later on another thread, exactly
        // once, including on cancellation. Scheduling must not block an HTTP
        // event loop. No HTTP types or version-specific DTOs cross this boundary.
        virtual void pronunciation(PronunciationInput, std::stop_token, Completion<PronunciationOutput>) const = 0;
        virtual void phoneme(PhonemeInput, std::stop_token, Completion<PhonemeOutput>) const = 0;
        virtual void duration(DurationInput, std::stop_token, Completion<DurationOutput>) const = 0;
        virtual void parameter(ParameterInput, std::stop_token, Completion<ParameterOutput>) const = 0;
        virtual void audio(AudioInput, std::stop_token, Completion<AudioOutput>) const = 0;
    };

    using ArchitecturePtr = std::shared_ptr<const Architecture>;

}

#endif // DSSP_ARCH_ARCHITECTURE_H
