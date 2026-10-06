# Adapted from ds-editor-lite's cmake/OrtRuntimeGate.cmake at
# 1671bc7c9e96c6cb3841569a62f4205b4c85f0b5. This only manages the driver's CUDA
# payload; it does not inspect or deploy executable/library dependencies.
cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED expect_cuda OR NOT DEFINED runtime_dir)
    message(FATAL_ERROR "OrtRuntimeGate: expect_cuda and runtime_dir are required")
endif()

set(_cuda_dir "${runtime_dir}/cuda")
if(expect_cuda)
    if(NOT IS_DIRECTORY "${_cuda_dir}")
        message(FATAL_ERROR
            "DSSP_ENABLE_CUDA=ON but the CUDA payload is missing: ${_cuda_dir}. "
            "Install the cuda12 manifest feature before building.")
    endif()
elseif(IS_DIRECTORY "${_cuda_dir}")
    # Validate the resolved child directory before removing a stale payload.
    file(REAL_PATH "${runtime_dir}" _real_runtime_dir)
    file(REAL_PATH "${_cuda_dir}" _real_cuda_dir)
    file(RELATIVE_PATH _cuda_relative "${_real_runtime_dir}" "${_real_cuda_dir}")
    if(NOT _cuda_relative STREQUAL "cuda")
        message(FATAL_ERROR "CUDA payload resolves outside its expected runtime directory")
    endif()
    message(STATUS "DSSP_ENABLE_CUDA=OFF: removing stale CUDA payload ${_cuda_dir}")
    file(REMOVE_RECURSE "${_real_cuda_dir}")
endif()
