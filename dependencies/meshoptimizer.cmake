cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    extern_meshoptimizer

    GIT_REPOSITORY https://github.com/zeux/meshoptimizer
    GIT_TAG        v0.22
)

FetchContent_GetProperties(extern_meshoptimizer)

if(NOT extern_meshoptimizer_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    message(STATUS "Fetching meshoptimizer...")
    FetchContent_Populate(extern_meshoptimizer)
    include_directories(${extern_meshoptimizer_SOURCE_DIR}/src)
    add_subdirectory(
        ${extern_meshoptimizer_SOURCE_DIR}
        ${extern_meshoptimizer_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
endif()
