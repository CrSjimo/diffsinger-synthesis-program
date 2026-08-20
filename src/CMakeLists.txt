include("conf.cmake")

dssp_init_buildsystem()

# ----------------------------------
# Main Application
# ----------------------------------
add_subdirectory(app)

# ----------------------------------
# Libraries
# ----------------------------------
add_subdirectory(libs)

# ----------------------------------
# Tests
# ----------------------------------
if(DSSP_BUILD_TESTS)
    enable_testing()

    add_subdirectory(tests)
endif()

# ----------------------------------
# Install
# ----------------------------------
dssp_install()

dssp_finish_buildsystem()
