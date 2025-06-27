cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    extern_googletest

    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG        v1.17.0
)

FetchContent_GetProperties(extern_googletest)

if(NOT extern_googletest_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    message(STATUS "Fetching googletest...")
    FetchContent_Populate(extern_googletest)
    include_directories(${extern_googletest_SOURCE_DIR}/googletest/include)
    add_subdirectory(
        ${extern_googletest_SOURCE_DIR}
        ${extern_googletest_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
endif()
