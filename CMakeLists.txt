cmake_minimum_required(VERSION 3.22.1)

project(gltf VERSION 0.1.0)

include(CTest)

configure_file(src/gltf_config.h.in gltf_config.h)

set(CMAKE_CXX_STANDARD 17)

option(GLTF_BUILD_TEST "Enable building tests" OFF)

include_directories(inc)

message(STATUS ${PROJECT_NAME})

include(dependencies/vector_math.cmake)

add_library(${PROJECT_NAME} SHARED
    src/gltf.cpp src/base64.cpp src/utils.cpp
)
target_link_libraries(${PROJECT_NAME} 
    vector_math
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
    include(dependencies/googletest.cmake)

    add_executable(${PROJECT_NAME}_test 
        test/main.cpp
    )
    target_link_libraries(${PROJECT_NAME}_test 
        ${PROJECT_NAME}
        gtest
    )
    add_test(NAME gltf_test COMMAND ${PROJECT_NAME}_test)
    #add_test(NAME gltf_test2 COMMAND ${PROJECT_NAME}_test)
    list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
endif()
