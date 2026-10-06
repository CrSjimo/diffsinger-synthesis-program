find_package(stdcorelib CONFIG REQUIRED)
find_package(Drogon CONFIG REQUIRED)
find_package(synthrt CONFIG REQUIRED)
find_package(dsinfer CONFIG REQUIRED)
find_package(wolf CONFIG REQUIRED)
find_package(otter CONFIG REQUIRED)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.cpp *.h)

dssp_add_executable(dssp
    SOURCES ${_src}
)

set_target_properties(dssp PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_definitions(dssp PRIVATE DSSP_DESCRIPTION="${DSSP_DESCRIPTION}")
target_compile_definitions(dssp PRIVATE DSSP_VERSION="${DSSP_VERSION}")
if(DSSP_ENABLE_CUDA)
    target_compile_definitions(dssp PRIVATE DSSP_ENABLE_CUDA)
endif()

dssp_sync_include(dssp
    PREFIX dssp
    FORCE
)

target_link_libraries(dssp PRIVATE
    stdcorelib::stdcorelib
    Drogon::Drogon
    codecutil
    synthrt::synthrt
    dsinfer::dsinfer
    wolf::wolf
    otter::otter
    $<$<PLATFORM_ID:Windows>:dxgi.lib>
)

include("${DSSP_SOURCE_DIR}/cmake/DeployInference.cmake")
dssp_deploy_inference_plugins(dssp)
