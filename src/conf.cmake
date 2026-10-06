# ----------------------------------
# Project Constants
# ----------------------------------
# A development install is needed to install synced library headers.
set(DSSP_DEVEL OFF)

# The build helpers default to shared libraries. This project is a plain
# executable, so disable them to keep the setup explicit.
set(DSSP_BUILD_SHARED OFF)
set(DSSP_BUILD_STATIC OFF)

# Keep the GNU layout on every platform, including macOS. Synced headers are
# build-time references and live outside the application's deployment tree.
set(DSSP_MACOSX_BUNDLE_NAME "")
set(DSSP_BUILD_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
set(DSSP_BUILD_RUNTIME_DIR "${QMSETUP_BUILD_DIR}/bin")
set(DSSP_BUILD_LIBRARY_DIR "${QMSETUP_BUILD_DIR}/lib")
set(DSSP_BUILD_PLUGINS_DIR "${DSSP_BUILD_LIBRARY_DIR}/plugins")
set(DSSP_BUILD_SHARE_DIR "${QMSETUP_BUILD_DIR}/share")
set(DSSP_BUILD_DATA_DIR "${DSSP_BUILD_SHARE_DIR}")

set(DSSP_INSTALL_RUNTIME_DIR bin)
set(DSSP_INSTALL_LIBRARY_DIR lib)
set(DSSP_INSTALL_PLUGINS_DIR "${DSSP_INSTALL_LIBRARY_DIR}/plugins")
set(DSSP_INSTALL_SHARE_DIR share)
set(DSSP_INSTALL_DATA_DIR "${DSSP_INSTALL_SHARE_DIR}")
# Each library publishes its own complete flatcase prefix.
set(DSSP_INSTALL_INCLUDE_DIR include)

# ----------------------------------
# Include Build Helpers
# ----------------------------------
qm_import(private/BuildSystem)
qm_setup_build_repo_helpers(dssp)
