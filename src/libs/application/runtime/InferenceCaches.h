#ifndef DSSP_RUNTIME_INFERENCECACHES_H
#define DSSP_RUNTIME_INFERENCECACHES_H

#include <array>
#include <map>
#include <string>
#include <string_view>

#include <dsspresource/CacheOptions.h>

namespace dssp::runtime {
    struct InferenceCacheGroup {
        std::string_view id;
        std::string_view environmentPrefix;
        std::string_view description;
    };
    inline constexpr std::array InferenceCacheGroups{
        InferenceCacheGroup{"diffsinger-pronunciation", "DSSP_DIFFSINGER_PRONUNCIATION", "DiffSinger pronunciation"},
        InferenceCacheGroup{"diffsinger-phoneme", "DSSP_DIFFSINGER_PHONEME", "DiffSinger phoneme"},
        InferenceCacheGroup{"diffsinger-onset", "DSSP_DIFFSINGER_ONSET", "DiffSinger onset"},
        InferenceCacheGroup{"diffsinger-duration", "DSSP_DIFFSINGER_DURATION", "DiffSinger duration"},
        InferenceCacheGroup{"diffsinger-pitch", "DSSP_DIFFSINGER_PITCH", "DiffSinger pitch"},
        InferenceCacheGroup{"diffsinger-variance", "DSSP_DIFFSINGER_VARIANCE", "DiffSinger variance"},
        InferenceCacheGroup{"diffsinger-acoustic", "DSSP_DIFFSINGER_ACOUSTIC", "DiffSinger acoustic"},
        InferenceCacheGroup{"diffsinger-vocoder", "DSSP_DIFFSINGER_VOCODER", "DiffSinger vocoder"},
        InferenceCacheGroup{"extraction-note", "DSSP_EXTRACTION_NOTE", "Note extraction"},
        InferenceCacheGroup{"extraction-pitch", "DSSP_EXTRACTION_PITCH", "Pitch extraction"},
        InferenceCacheGroup{"extraction-separation", "DSSP_EXTRACTION_SEPARATION", "Audio separation"}
    };
    inline std::map<std::string, resource::CacheOptions> defaultInferenceCaches() {
        std::map<std::string, resource::CacheOptions> result;
        for (const auto &group : InferenceCacheGroups) {
            result.emplace(group.id, resource::CacheOptions{});
        }
        return result;
    }
}

#endif // DSSP_RUNTIME_INFERENCECACHES_H
