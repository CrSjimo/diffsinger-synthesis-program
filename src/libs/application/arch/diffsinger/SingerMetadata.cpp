#include "SingerMetadata.h"

#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

namespace dssp::arch_diffsinger {

    SingerMetadata singerMetadata(const ds::Api::DiffSinger::L1::DiffSingerConfiguration &) {
        // TODO: read the mixing permission when dsinfer exposes singer metadata.
        // Checked against Synthrt/dsinfer 63bef253607649dcface0b37abc68f051cc45c8b:
        // no permission field exists. Retain the agreed default of allowing it.
        return {};
    }
}
