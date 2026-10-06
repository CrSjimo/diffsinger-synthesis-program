find_package(dsinfer CONFIG REQUIRED)

file(GLOB _src CONFIGURE_DEPENDS *.cpp *.h)
add_library(dssp_arch_diffsinger STATIC ${_src})
target_compile_features(dssp_arch_diffsinger PUBLIC cxx_std_23)
target_include_directories(dssp_arch_diffsinger PUBLIC "$<BUILD_INTERFACE:${DSSP_BUILD_INCLUDE_DIR}>")
target_link_libraries(dssp_arch_diffsinger PUBLIC dssp_arch dssp_inference dssp_media dssp_localization)
target_link_libraries(dssp_arch_diffsinger PRIVATE stdcorelib::stdcorelib dsinfer::dsinfer)
dssp_sync_include(dssp_arch_diffsinger PREFIX dssparchdiffsinger FORCE)

dssp_add_translations(dssp_arch_diffsinger "${CMAKE_CURRENT_SOURCE_DIR}/translations" "${DSSP_RESOURCE_BUILD_DIR}/localization/diffsinger")
