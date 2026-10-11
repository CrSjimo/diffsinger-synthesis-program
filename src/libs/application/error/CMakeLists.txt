
file(GLOB _src CONFIGURE_DEPENDS *.cpp *.h)
add_library(dssp_error STATIC ${_src})

target_compile_features(dssp_error PUBLIC cxx_std_23)
target_include_directories(dssp_error PUBLIC "$<BUILD_INTERFACE:${DSSP_BUILD_INCLUDE_DIR}>")

dssp_sync_include(dssp_error PREFIX dssperror FORCE)
