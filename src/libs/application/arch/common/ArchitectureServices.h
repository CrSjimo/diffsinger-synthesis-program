#ifndef DSSP_ARCH_ARCHITECTURESERVICES_H
#define DSSP_ARCH_ARCHITECTURESERVICES_H

#include <filesystem>
#include <memory>

namespace dssp::inference { class Environment; }
namespace dssp::media { class Resources; }

namespace dssp::arch {
    // The composition root supplies shared infrastructure. Architecture factories
    // never create their own unit/driver or own global registration state.
    struct ArchitectureServices {
        std::shared_ptr<inference::Environment> inference;
        std::shared_ptr<media::Resources> media;
        std::filesystem::path translationRoot;
    };
}

#endif // DSSP_ARCH_ARCHITECTURESERVICES_H
