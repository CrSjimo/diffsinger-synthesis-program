#include "SingerMetadata.h"

#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

namespace dssp::arch_diffsinger {

    SingerMetadata singerMetadata(const ds::Api::DiffSinger::L1::DiffSingerConfiguration &) {
        // TODO: read the mixing permission when dsinfer exposes singer metadata.
        // Checked against Synthrt/dsinfer 0e9766235b97c7af65b2030d33fe922abaa73c4f:
        // no permission field exists. Retain the agreed default of allowing it.
        return {};
    }
}
