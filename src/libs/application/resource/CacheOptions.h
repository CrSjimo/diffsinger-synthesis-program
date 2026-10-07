#ifndef DSSP_RESOURCE_CACHEOPTIONS_H
#define DSSP_RESOURCE_CACHEOPTIONS_H

#include <chrono>
#include <cstddef>

namespace dssp::resource {

    struct CacheOptions {
        // Zero disables the count limit or idle expiry, respectively.
        std::size_t capacity = 16;
        std::chrono::milliseconds idleTimeout{600000};
        std::chrono::milliseconds cleanupInterval{60000};
    };
}

#endif // DSSP_RESOURCE_CACHEOPTIONS_H
