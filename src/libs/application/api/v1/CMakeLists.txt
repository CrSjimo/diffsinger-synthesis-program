
file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.cpp *.h)
add_library(dssp_api_v1 STATIC ${_src})
target_compile_features(dssp_api_v1 PUBLIC cxx_std_23)
target_include_directories(dssp_api_v1 PUBLIC "$<BUILD_INTERFACE:${DSSP_BUILD_INCLUDE_DIR}>")
target_link_libraries(dssp_api_v1 PUBLIC dssp_arch dssp_http dssp_extraction)
# Dispatch templates log through stdcorelib in their synchronized headers.
target_link_libraries(dssp_api_v1 PUBLIC stdcorelib::stdcorelib)
dssp_sync_include(dssp_api_v1 PREFIX dsspapiv1 FORCE)
