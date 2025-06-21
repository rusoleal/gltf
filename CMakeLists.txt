cmake_minimum_required(VERSION 3.22.1)

project(gltf VERSION 0.0.11)

configure_file(src/gltf_config.h.in gltf_config.h)

set(CMAKE_CXX_STANDARD 17)

include_directories(inc)

message(STATUS ${PROJECT_NAME})

add_library(${PROJECT_NAME} SHARED
    src/gltf.cpp src/base64.cpp
)

target_include_directories (gltf PUBLIC 
                            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>"
                            )

target_include_directories(gltf PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )