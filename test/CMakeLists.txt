cmake_minimum_required(VERSION 3.22.1)

include(CTest)

set(CMAKE_CXX_STANDARD 17)

include_directories(../inc)

add_executable(${PROJECT_NAME}_test
    main.cpp
)
target_link_libraries(${PROJECT_NAME}_test ${PROJECT_NAME})

add_test(NAME gltf_test COMMAND test/${PROJECT_NAME}_test)