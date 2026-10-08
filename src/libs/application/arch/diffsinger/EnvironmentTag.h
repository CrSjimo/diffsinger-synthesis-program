#ifndef DSSP_ARCHDIFFSINGER_ENVIRONMENTTAG_H
#define DSSP_ARCHDIFFSINGER_ENVIRONMENTTAG_H

#include <dssparchdiffsinger/SingerCatalog.h>

namespace dssp::arch_diffsinger {
    std::expected<std::string, arch::EnvironmentErrors> synthesisEnvironmentTag(const SingerCatalog &, const arch::MultiSingerContext &);
}

#endif // DSSP_ARCHDIFFSINGER_ENVIRONMENTTAG_H
