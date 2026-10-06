#include "SingerMetadata.h"

#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

namespace dssp::arch_diffsinger {

    SingerMetadata singerMetadata(const ds::Api::DiffSinger::L1::DiffSingerConfiguration &) {
        // The pinned dsinfer singer metadata has no mixing permission yet.
        // Read its future field here; until then, permission defaults to allowed.
        return {};
    }
}
