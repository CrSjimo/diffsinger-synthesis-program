#include "LanguageMetadata.h"

namespace dssp::inference {

    LanguageMetadata languageMetadata(const wolf::LinguistSpec &language) {
        // TODO: read the default lyric when Wolf exposes it as language metadata.
        // Checked against eda2b4880da39dee9d9e9cfd7e04caaf951993e7: the
        // declaration has no such field. G2P fallback defaultPronunciation is
        // a conversion policy, not the language's default lyric.
        return {language.name(), {}};
    }
}
