file(GLOB_RECURSE _headers CONFIGURE_DEPENDS *.h)

dssp_add_library(codecutil INTERFACE)

target_sources(codecutil INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${_headers}
)

set_target_properties(codecutil PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

dssp_sync_include(codecutil
    PREFIX codecutil
    FORCE
)
