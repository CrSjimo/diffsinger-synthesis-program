#ifndef DSSP_ARCHDIFFSINGER_METADATA_H
#define DSSP_ARCHDIFFSINGER_METADATA_H

#include <dssparch/Errors.h>
#include <dssparch/Types.h>
#include <dssplocalization/Catalog.h>

namespace dssp::arch_diffsinger {
    arch::CatalogResult<arch::ArchitectureMetadata> architectureMetadata(const arch::CatalogOptions &, const localization::Catalog &);
}

#endif // DSSP_ARCHDIFFSINGER_METADATA_H
