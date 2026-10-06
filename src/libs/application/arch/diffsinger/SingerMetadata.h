#ifndef DSSP_ARCHDIFFSINGER_SINGERMETADATA_H
#define DSSP_ARCHDIFFSINGER_SINGERMETADATA_H

namespace ds::Api::DiffSinger::L1 {
    class DiffSingerConfiguration;
}

namespace dssp::arch_diffsinger {

    struct SingerMetadata {
        bool permitsMixing = true;
    };

    SingerMetadata singerMetadata(const ds::Api::DiffSinger::L1::DiffSingerConfiguration &);
}

#endif // DSSP_ARCHDIFFSINGER_SINGERMETADATA_H
