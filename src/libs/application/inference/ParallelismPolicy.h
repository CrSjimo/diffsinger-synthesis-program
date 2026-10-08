#ifndef DSSP_INFERENCE_PARALLELISMPOLICY_H
#define DSSP_INFERENCE_PARALLELISMPOLICY_H

#include <string_view>

#ifdef _WIN32
#    pragma push_macro("interface")
#    undef interface
#endif
#include <synthrt/Core/ContribSpec.h>
#ifdef _WIN32
#    pragma pop_macro("interface")
#endif

namespace dssp::inference {
    // This is a host whitelist, not an inference/provider concurrency contract.
    // Unknown interfaces, levels, variants, unresolved imports and cycles require
    // serialization. Updating providers never implicitly enlarges the whitelist.
    bool isParallelLeaf(std::string_view interfaceId, int level, std::string_view variant);
    bool permitsParallelExecution(const srt::ContribSpec &);
}

#endif // DSSP_INFERENCE_PARALLELISMPOLICY_H
