#ifndef DSSP_INFERENCE_LANGUAGEROUTE_H
#define DSSP_INFERENCE_LANGUAGEROUTE_H

#include <optional>
#include <string_view>

// Preserve the caller's Windows SDK macro while parsing synthrt's interface().
#ifdef _WIN32
#    pragma push_macro("interface")
#    undef interface
#endif

#include <synthrt/SVS/SingerContrib.h>
#include <wolf/Api/Inferences/Common/1/CommonApiL1.h>
#include <wolf/Linguist/LinguistContrib.h>

#ifdef _WIN32
#    pragma pop_macro("interface")
#endif

#include <dsspinference/InferenceError.h>

namespace dssp::inference {

    enum class LanguageDepth {
        Pronunciation,
        Phonemes,
        Onsets
    };

    // Immutable, already-resolved import views. The environment retains packages;
    // inspecting a route creates neither a pipeline nor an inference executive.
    struct LanguageRoute {
        const wolf::LinguistSpec *spec = nullptr;
        wolf::Api::Common::L1::LanguageScheme binding;
        std::optional<srt::ContribImport> g2p;
        std::optional<srt::ContribImport> s2p;
        std::optional<srt::ContribImport> onset;
        LanguageDepth depth = LanguageDepth::Pronunciation;
    };

    std::expected<LanguageRoute, ResourceError> readLanguageRoute(const srt::SingerSpec &, std::string_view handle);
}

#endif // DSSP_INFERENCE_LANGUAGEROUTE_H
