cmake_minimum_required(VERSION 3.22.1)

project("gltf")

set(CMAKE_CXX_STANDARD 17)

include_directories(inc)

message(STATUS ${PROJECT_NAME})

add_library(${PROJECT_NAME} SHARED
    src/gltf.cpp
)
