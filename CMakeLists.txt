cmake_minimum_required(VERSION 3.22.1)

project(gltf VERSION 0.0.16)

include(CTest)

configure_file(src/gltf_config.h.in gltf_config.h)

set(CMAKE_CXX_STANDARD 17)

option(GLTF_BUILD_TEST "Enable building tests" OFF)

include_directories(inc)

message(STATUS ${PROJECT_NAME})

add_library(${PROJECT_NAME} SHARED
    src/gltf.cpp src/base64.cpp src/utils.cpp
)

target_include_directories (gltf PUBLIC 
                            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>"
                            )

target_include_directories(gltf PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )

if (GLTF_BUILD_TEST)
    # googletest
    SET(BUILD_GMOCK OFF CACHE BOOL "DISABLE GMOCK")
    SET(INSTALL_GTEST OFF CACHE BOOL "DISABLE INSTALL_GTEST")
    include(googletest.cmake)

    # curl
    find_package(CURL)
    #include(curl.cmake)

    add_executable(${PROJECT_NAME}_test 
        test/main.cpp
    )
    target_link_libraries(${PROJECT_NAME}_test 
        ${PROJECT_NAME}
        gtest
        CURL::libcurl
    )
    add_test(NAME gltf_test COMMAND ${PROJECT_NAME}_test)
    #add_test(NAME gltf_test2 COMMAND ${PROJECT_NAME}_test)
    list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
endif()
