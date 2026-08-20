find_package(CLI11 CONFIG REQUIRED)
find_package(stdcorelib CONFIG REQUIRED)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.cpp *.h)

dssp_add_application(dssp
    SOURCES ${_src}
)

if(DSSP_ENABLE_CUDA)
    target_compile_definitions(dssp PRIVATE DSSP_ENABLE_CUDA)
endif()

dssp_sync_include(dssp
    PREFIX dssp
    FORCE
)

target_link_libraries(dssp PRIVATE
    CLI11::CLI11
    stdcorelib::stdcorelib
    $<$<PLATFORM_ID:Windows>:dxgi.lib>
)
