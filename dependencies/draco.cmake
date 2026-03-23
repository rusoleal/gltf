cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    extern_draco

    GIT_REPOSITORY https://github.com/google/draco
    GIT_TAG        1.5.7
)

FetchContent_GetProperties(extern_draco)

if(NOT extern_draco_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    #set(DRACO_TESTS   OFF CACHE BOOL "" FORCE)
    #set(DRACO_JS_GLUE OFF CACHE BOOL "" FORCE)

    message(STATUS "Fetching draco...")
    FetchContent_Populate(extern_draco)
    include_directories(${extern_draco_SOURCE_DIR}/src)
    include_directories(${CMAKE_BINARY_DIR})
    add_subdirectory(
        ${extern_draco_SOURCE_DIR}
        ${extern_draco_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )

    # On MSVC, draco names its library target 'draco' (not 'draco_static').
    # Create an alias so gltf's target_link_libraries(... draco_static) works.
    if(MSVC AND TARGET draco AND NOT TARGET draco_static)
        add_library(draco_static ALIAS draco)
    endif()
endif()
