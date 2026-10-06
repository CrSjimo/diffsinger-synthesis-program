#include "LanguageMetadata.h"

namespace dssp::inference {

    LanguageMetadata languageMetadata(const wolf::LinguistSpec &language) {
        // The pinned Wolf contract exposes the name but no default lyric yet.
        // Read its future field here; until then, retain an empty value.
        return {language.name(), {}};
    }
}
