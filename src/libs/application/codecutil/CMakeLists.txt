add_library(dssp_codecutil INTERFACE)

target_compile_features(dssp_codecutil INTERFACE cxx_std_23)
target_include_directories(dssp_codecutil INTERFACE "$<BUILD_INTERFACE:${DSSP_BUILD_INCLUDE_DIR}>")
target_sources(dssp_codecutil INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/codecutil.h>")

dssp_sync_include(dssp_codecutil PREFIX dsspcodecutil FORCE)
