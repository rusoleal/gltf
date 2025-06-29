cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    extern_vector_math

    GIT_REPOSITORY https://github.com/rusoleal/vector_math
    GIT_TAG        v0.0.1
)

FetchContent_GetProperties(extern_vector_math)

if(NOT extern_vector_math_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    message(STATUS "Fetching vector_math...")
    FetchContent_Populate(extern_vector_math)
    include_directories(${extern_vector_math_SOURCE_DIR}/inc)
    add_subdirectory(
        ${extern_vector_math_SOURCE_DIR}
        ${extern_vector_math_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
endif()
