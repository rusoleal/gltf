cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    extern_curl

    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        8.14.1
)

FetchContent_GetProperties(extern_curl)

if(NOT extern_curl_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    message(STATUS "Fetching curl...")
    FetchContent_Populate(extern_curl)
    include_directories(${extern_curl_SOURCE_DIR}/src)
    add_subdirectory(
        ${extern_curl_SOURCE_DIR}
        ${extern_curl_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
endif()
