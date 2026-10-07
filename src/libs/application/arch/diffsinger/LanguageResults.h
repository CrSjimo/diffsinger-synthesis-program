#ifndef DSSP_ARCHDIFFSINGER_LANGUAGERESULTS_H
#define DSSP_ARCHDIFFSINGER_LANGUAGERESULTS_H

#include <wolf/Api/Inferences/G2P/1/G2PApiL1.h>

#include <dssparch/Errors.h>
#include <dssparch/Types.h>

namespace dssp::arch_diffsinger {
    arch::SynthesisResult<arch::Pronunciation> translatePronunciation(const wolf::Api::G2P::L1::G2PWordOutput &, const std::string &lyric);
    std::vector<arch::Phoneme> slicePhonemes(std::vector<arch::Phoneme>, const arch::PronunciationNote &);
}

#endif // DSSP_ARCHDIFFSINGER_LANGUAGERESULTS_H
