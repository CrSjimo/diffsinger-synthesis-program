include_guard(GLOBAL)

set(DSSP_WOLF_LANG_PACKAGES "" CACHE PATH
    "Directory containing unpacked Wolf language packages (each with desc.json)")

function(dssp_deploy_inference_plugins target)
    find_package(onnxruntime-builds CONFIG REQUIRED)

    # The upstream packages install plugins separately from their link targets.
    get_filename_component(_vcpkg_prefix "${ONNXRUNTIME_BUILDS_INCLUDE_DIR}" DIRECTORY)
    set(_library_source "${_vcpkg_prefix}/$<$<CONFIG:Debug>:debug/>lib")

    if(WIN32)
        set(_deploy_root "$<TARGET_FILE_DIR:${target}>")
        set(_runtime_source "${_vcpkg_prefix}/$<$<CONFIG:Debug>:debug/>bin")
    else()
        # This is a command-line executable, including on macOS.
        set(_deploy_root "$<TARGET_FILE_DIR:${target}>/../lib")
        set(_runtime_source "${_library_source}")
    endif()

    if(DSSP_INSTALL)
        set(_copy_args INSTALL_DIR .)
    else()
        set(_copy_args SKIP_INSTALL)
    endif()

    # Categories and driver bundles are loaded at runtime, so linking the four
    # libraries alone cannot deploy them. Preserve their upstream directory shape.
    foreach(_library IN ITEMS dsinfer wolf otter)
        qm_add_copy_command(${target}
            SOURCES "${_library_source}/plugins/${_library}/"
            DESTINATION "${_deploy_root}/plugins/${_library}"
            ${_copy_args}
        )
    endforeach()

    set(_ort_destination "${_deploy_root}/plugins/dsinfer/inferencedrivers/onnx/runtime")
    if(NOT IS_DIRECTORY "${ONNXRUNTIME_BUILDS_RUNTIME_DIR}")
        message(FATAL_ERROR "The onnxruntime-builds default runtime payload is missing.")
    endif()
    qm_add_copy_command(${target}
        SOURCES "${ONNXRUNTIME_BUILDS_RUNTIME_DIR}/"
        DESTINATION "${_ort_destination}"
        ${_copy_args}
    )

    if(DSSP_ENABLE_CUDA)
        if(NOT IS_DIRECTORY "${ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR}")
            message(FATAL_ERROR
                "DSSP_ENABLE_CUDA requires vcpkg install --x-feature=cuda12. "
                "Reinstall synthrt with this feature, or set DSSP_ENABLE_CUDA=OFF.")
        endif()
        qm_add_copy_command(${target}
            SOURCES "${ONNXRUNTIME_BUILDS_CUDA_RUNTIME_DIR}/"
            DESTINATION "${_ort_destination}/cuda"
            ${_copy_args}
        )
    endif()

    # Package data is prepared separately from building Wolf. Copy only package
    # directories, leaving archives and duplicate unpacked trees out of discovery.
    set(_language_source "${DSSP_WOLF_LANG_PACKAGES}")
    if(NOT _language_source AND DEFINED ENV{WOLF_LANG_PACKAGES_SOURCE})
        set(_language_source "$ENV{WOLF_LANG_PACKAGES_SOURCE}")
    endif()
    if(_language_source)
        if(NOT IS_DIRECTORY "${_language_source}")
            message(FATAL_ERROR "Wolf language package directory does not exist: ${_language_source}")
        endif()
        file(GLOB _manifests CONFIGURE_DEPENDS "${_language_source}/*/desc.json")
        foreach(_manifest IN LISTS _manifests)
            get_filename_component(_package_dir "${_manifest}" DIRECTORY)
            get_filename_component(_package_name "${_package_dir}" NAME)
            qm_add_copy_command(${target}
                SOURCES "${_package_dir}/"
                DESTINATION "${_deploy_root}/wolf/packages/${_package_name}"
                ${_copy_args}
            )
        endforeach()
    endif()

    # Dynamically loaded plugins have dependencies that the executable's
    # deployment cannot discover. Let qmsetup resolve those plugin binaries;
    # do not maintain a separate list for cpp-pinyin, LuaJIT, or future plugins.
    if(NOT QMSETUP_CORECMD_EXECUTABLE)
        message(FATAL_ERROR "Inference plugin deployment requires qmsetup's qmcorecmd tool.")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-Dqmcorecmd=${QMSETUP_CORECMD_EXECUTABLE}"
            "-Dplugin_root=${_deploy_root}/plugins"
            "-Dlibrary_dir=${_runtime_source}"
            "-Ddeploy_dir=${_deploy_root}"
            -P "${DSSP_SOURCE_DIR}/cmake/DeployPluginDependencies.cmake"
        COMMENT "Deploy inference plugin dependencies with qmsetup"
        VERBATIM
    )

    # Changing the CUDA option must also remove a payload staged by an earlier build.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-Dexpect_cuda=$<BOOL:${DSSP_ENABLE_CUDA}>"
            "-Druntime_dir=${_ort_destination}"
            -P "${DSSP_SOURCE_DIR}/cmake/OrtRuntimeGate.cmake"
        COMMENT "Gate ONNX Runtime CUDA payload (DSSP_ENABLE_CUDA=${DSSP_ENABLE_CUDA})"
        VERBATIM
    )

    if(DSSP_INSTALL)
        if(WIN32)
            set(_install_plugin_root "${DSSP_INSTALL_RUNTIME_DIR}")
        else()
            set(_install_plugin_root "${DSSP_INSTALL_LIBRARY_DIR}")
        endif()
        install(CODE "
            execute_process(
                COMMAND \"${CMAKE_COMMAND}\"
                    \"-Dqmcorecmd=${QMSETUP_CORECMD_EXECUTABLE}\"
                    \"-Dplugin_root=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_install_plugin_root}/plugins\"
                    \"-Dlibrary_dir=${_runtime_source}\"
                    \"-Ddeploy_dir=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_install_plugin_root}\"
                    -P \"${DSSP_SOURCE_DIR}/cmake/DeployPluginDependencies.cmake\"
                COMMAND_ERROR_IS_FATAL ANY
            )
            execute_process(
                COMMAND \"${CMAKE_COMMAND}\"
                    -Dexpect_cuda=${DSSP_ENABLE_CUDA}
                    \"-Druntime_dir=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_install_plugin_root}/plugins/dsinfer/inferencedrivers/onnx/runtime\"
                    -P \"${DSSP_SOURCE_DIR}/cmake/OrtRuntimeGate.cmake\"
                COMMAND_ERROR_IS_FATAL ANY
            )
        ")
    endif()
endfunction()
