include("conf.cmake")

dssp_init_buildsystem()

# Generate application data directly in its deployment directory.
set(DSSP_RESOURCE_BUILD_DIR "${DSSP_BUILD_DATA_DIR}")

# ----------------------------------
# Libraries
# ----------------------------------
add_subdirectory(libs)

# ----------------------------------
# Main Application
# ----------------------------------
add_subdirectory(app)

# ----------------------------------
# Tests
# ----------------------------------
if(DSSP_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ----------------------------------
# Install
# ----------------------------------
dssp_install()

dssp_finish_buildsystem()
