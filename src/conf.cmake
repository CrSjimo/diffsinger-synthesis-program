# ----------------------------------
# Project Constants
# ----------------------------------
# A development install is needed so that the synced headers are installed to
# `include/dssp/` together with the CMake package files.
set(DSSP_DEVEL OFF)

# The build helpers default to shared libraries. This project is a plain
# executable, so disable them to keep the setup explicit.
set(DSSP_BUILD_SHARED OFF)
set(DSSP_BUILD_STATIC OFF)

if(DSSP_INSTALL)
    # The headers go under a directory named after the package, so that an
    # include reads `<dssp/...>`.
    set(DSSP_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR}/${DSSP_INSTALL_NAME})
endif()

# ----------------------------------
# Include Build Helpers
# ----------------------------------
qm_import(private/BuildSystem)
qm_setup_build_repo_helpers(dssp)
