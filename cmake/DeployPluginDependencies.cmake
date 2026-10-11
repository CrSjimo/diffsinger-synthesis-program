cmake_minimum_required(VERSION 3.19)

foreach(_argument IN ITEMS qmcorecmd plugin_root library_dir deploy_dir)
    if(NOT DEFINED ${_argument} OR "${${_argument}}" STREQUAL "")
        message(FATAL_ERROR "DeployPluginDependencies: ${_argument} is required")
    endif()
endforeach()

# Only binaries beside plugin metadata are inputs. In particular, the ONNX
# Runtime payload is a separate bundle and must retain its own layout.
set(_plugins)

foreach(_family IN ITEMS dsinfer wolf otter srtsepinfer srtbeatinfer)
    if(NOT IS_DIRECTORY "${plugin_root}/${_family}")
        message(FATAL_ERROR "Inference plugin directory is missing: ${plugin_root}/${_family}")
    endif()

    file(GLOB_RECURSE _manifests LIST_DIRECTORIES FALSE "${plugin_root}/${_family}/plugin.json")
    foreach(_manifest IN LISTS _manifests)
        get_filename_component(_directory "${_manifest}" DIRECTORY)
        file(GLOB _binaries LIST_DIRECTORIES FALSE
            "${_directory}/*.dll"
            "${_directory}/*.dylib"
            "${_directory}/*.so"
            "${_directory}/*.so.*"
        )
        list(APPEND _plugins ${_binaries})
    endforeach()
endforeach()

if(NOT _plugins)
    message(FATAL_ERROR "No inference plugin binaries found under ${plugin_root}")
endif()

# qmsetup owns dependency traversal, copying and platform-specific RPATH fixes.
# Its inputs are the staged plugins, never the application executable.
execute_process(
    COMMAND "${qmcorecmd}" deploy --standard --force
        --linkdir "${deploy_dir}" --linkdir "${library_dir}" --out "${deploy_dir}" ${_plugins}
    COMMAND_ERROR_IS_FATAL ANY
)
