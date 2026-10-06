#ifndef DSSP_INFERENCE_LANGUAGEMETADATA_H
#define DSSP_INFERENCE_LANGUAGEMETADATA_H

#include <string>

#include <synthrt/Support/DisplayText.h>
#include <wolf/Linguist/LinguistContrib.h>

namespace dssp::inference {

    struct LanguageMetadata {
        srt::DisplayText name;
        std::string defaultLyric;
    };

    LanguageMetadata languageMetadata(const wolf::LinguistSpec &);
}

#endif // DSSP_INFERENCE_LANGUAGEMETADATA_H
