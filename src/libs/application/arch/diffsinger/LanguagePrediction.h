#ifndef DSSP_ARCHDIFFSINGER_LANGUAGEPREDICTION_H
#define DSSP_ARCHDIFFSINGER_LANGUAGEPREDICTION_H

#include <dssparch/Architecture.h>
#include <dssparchdiffsinger/SingerCatalog.h>

namespace dssp::arch_diffsinger {
    void predictPronunciation(std::shared_ptr<const SingerCatalog>, arch::PronunciationInput, std::stop_token, arch::Completion<arch::PronunciationOutput>);
    void predictPhoneme(std::shared_ptr<const SingerCatalog>, arch::PhonemeInput, std::stop_token, arch::Completion<arch::PhonemeOutput>);
}

#endif // DSSP_ARCHDIFFSINGER_LANGUAGEPREDICTION_H
