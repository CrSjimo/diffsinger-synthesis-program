find_package(GTest CONFIG REQUIRED)
find_package(onnxruntime-builds CONFIG REQUIRED)

add_executable(dssp_tests
    ErrorTest.cpp
    MediaTypeTest.cpp
    RequestCodecTest.cpp
    ResponseTest.cpp
    ControllerTest.cpp
    ArchitectureDispatchTest.cpp
    ArchitectureMappingTest.cpp
    ArchitectureCatalogTest.cpp
    ServerConfigTest.cpp
    DeviceInfoTest.cpp
    RequestLifetimeTest.cpp
    AccessLogTest.cpp
    LogCapture.h
    TestFiles.h
    TestLanguagePackage.h
)
target_compile_features(dssp_tests PRIVATE cxx_std_23)
target_link_libraries(dssp_tests PRIVATE dssp_api_v1 dssp_runtime GTest::gtest_main)
dssp_sync_include(dssp_tests PREFIX dssptests FORCE NO_INSTALL)

# Real HTTP lifecycle coverage runs in a separate process: Drogon owns a singleton.
add_executable(dssp_http_access_log_tests HttpAccessLogTest.cpp)
target_compile_features(dssp_http_access_log_tests PRIVATE cxx_std_23)
target_link_libraries(dssp_http_access_log_tests PRIVATE dssp_api_v1 GTest::gtest_main)

# Backend and composition tests are isolated from the HTTP/API target.
add_executable(dssp_arch_diffsinger_tests DiffSingerTest.cpp SingerDiscoveryTest.cpp LanguagePredictionTest.cpp ../app/ArchComposition.cpp)
target_compile_features(dssp_arch_diffsinger_tests PRIVATE cxx_std_23)
target_link_libraries(dssp_arch_diffsinger_tests PRIVATE dssp_arch_diffsinger GTest::gtest_main)
target_compile_definitions(dssp_arch_diffsinger_tests PRIVATE DSSP_TEST_RESOURCE_DIR="${DSSP_RESOURCE_BUILD_DIR}")
get_filename_component(_test_vcpkg_prefix "${ONNXRUNTIME_BUILDS_INCLUDE_DIR}" DIRECTORY)
target_compile_definitions(dssp_arch_diffsinger_tests PRIVATE DSSP_TEST_PLUGIN_DIR="${_test_vcpkg_prefix}/$<$<CONFIG:Debug>:debug/>lib/plugins")

# This host test deliberately has no architecture target in its link graph.
add_executable(dssp_inference_tests InferenceTest.cpp InferencePoolTest.cpp)
target_compile_features(dssp_inference_tests PRIVATE cxx_std_23)
target_link_libraries(dssp_inference_tests PRIVATE dssp_inference GTest::gtest_main)
target_compile_definitions(dssp_inference_tests PRIVATE DSSP_TEST_PLUGIN_DIR="${_test_vcpkg_prefix}/$<$<CONFIG:Debug>:debug/>lib/plugins")

add_executable(dssp_resource_tests ResourceTest.cpp)
target_compile_features(dssp_resource_tests PRIVATE cxx_std_23)
target_link_libraries(dssp_resource_tests PRIVATE dssp_resource GTest::gtest_main)

add_executable(dssp_metadata_resources_tests CodecutilTest.cpp LocalizationTest.cpp MediaPathTest.cpp MediaResourceTest.cpp)
target_compile_features(dssp_metadata_resources_tests PRIVATE cxx_std_23)
target_link_libraries(dssp_metadata_resources_tests PRIVATE dssp_inference dssp_media dssp_codecutil GTest::gtest_main)
target_compile_definitions(dssp_metadata_resources_tests PRIVATE DSSP_TEST_RESOURCE_DIR="${DSSP_RESOURCE_BUILD_DIR}")
target_compile_definitions(dssp_metadata_resources_tests PRIVATE DSSP_TEST_TRANSLATION_DIR="${CMAKE_CURRENT_BINARY_DIR}/translations")
dssp_add_translations(dssp_metadata_resources_tests "${CMAKE_CURRENT_SOURCE_DIR}/translations" "${CMAKE_CURRENT_BINARY_DIR}/translations")

# Discovery happens only when tests are run, not during cross-compilation/build.
include(GoogleTest)
gtest_discover_tests(dssp_tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(dssp_http_access_log_tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(dssp_arch_diffsinger_tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(dssp_inference_tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(dssp_resource_tests DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(dssp_metadata_resources_tests DISCOVERY_MODE PRE_TEST)
