cmake_minimum_required(VERSION 3.22.1)

project(gltf VERSION 0.3.0)

include(CTest)

configure_file(src/gltf_config.h.in gltf_config.h)

set(CMAKE_CXX_STANDARD 17)

option(GLTF_BUILD_TEST  "Enable building tests"            OFF)
option(GLTF_BUILD_FUZZ  "Enable building libFuzzer targets" OFF)

# Floating-point precision used throughout the public API.
# Must match between the library build and any consumer that includes the headers.
set(GLTF_REAL_NUMBER_TYPE "double" CACHE STRING "Floating-point type used in the public API (float or double)")

message(STATUS ${PROJECT_NAME})

include(dependencies/vector_math.cmake)
set(DRACO_BUILD_ENCODER OFF)
include(dependencies/draco.cmake)
include(dependencies/meshoptimizer.cmake)

add_library(${PROJECT_NAME} SHARED
    src/gltf.cpp src/base64.cpp src/utils.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    vector_math
    draco_static
    meshoptimizer
)
target_compile_definitions(${PROJECT_NAME} PUBLIC
    GLTF_REAL_NUMBER_TYPE=${GLTF_REAL_NUMBER_TYPE}
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

target_include_directories(gltf PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>"
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# ── Install ────────────────────────────────────────────────────────────────

install(TARGETS gltf
    EXPORT gltfTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Public headers
install(DIRECTORY inc/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Generated version header
install(FILES "${PROJECT_BINARY_DIR}/gltf_config.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# vector_math headers (types appear in gltf public API)
install(DIRECTORY "${extern_vector_math_SOURCE_DIR}/inc/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# CMake package config files
install(EXPORT gltfTargets
    FILE gltfTargets.cmake
    NAMESPACE gltf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gltf
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gltfConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/gltfConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/gltf"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/gltfConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/gltfConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/gltfConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gltf
)

if (GLTF_BUILD_FUZZ)
    # Coverage instrumentation (-fsanitize=fuzzer-no-link) still requires
    # clang. The fuzzer ENGINE runtime is compiled from source (no pre-install).
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-fsanitize=fuzzer-no-link" COMPILER_SUPPORTS_FUZZER_NO_LINK)
    if (NOT COMPILER_SUPPORTS_FUZZER_NO_LINK)
        message(FATAL_ERROR
            "GLTF_BUILD_FUZZ requires a clang compiler that supports "
            "-fsanitize=fuzzer-no-link (coverage instrumentation). "
            "Use clang/clang++. The fuzzer runtime is built from source automatically.")
    endif()

    # Fetch and build the libFuzzer runtime from LLVM compiler-rt sources.
    include(dependencies/libfuzzer.cmake)

    # -fsanitize=fuzzer-no-link  → instrument code for coverage (no runtime linked)
    # -fsanitize=address         → AddressSanitizer for memory error detection
    set(FUZZ_COMPILE_FLAGS "-fsanitize=fuzzer-no-link,address" "-fno-omit-frame-pointer" "-g" "-O1")
    set(FUZZ_LINK_FLAGS    "-fsanitize=address")

    foreach(TARGET fuzz_gltf fuzz_glb)
        add_executable(${PROJECT_NAME}_${TARGET} fuzz/${TARGET}.cpp)
        target_link_libraries(${PROJECT_NAME}_${TARGET} ${PROJECT_NAME} fuzzer_runtime)
        target_compile_options(${PROJECT_NAME}_${TARGET} PRIVATE ${FUZZ_COMPILE_FLAGS})
        target_link_options(${PROJECT_NAME}_${TARGET}    PRIVATE ${FUZZ_LINK_FLAGS})
    endforeach()

    # Copy seed corpus next to the binaries so fuzz targets can be run directly.
    file(COPY fuzz/corpus DESTINATION ${CMAKE_BINARY_DIR})
    message(STATUS "libFuzzer targets: ${PROJECT_NAME}_fuzz_gltf, ${PROJECT_NAME}_fuzz_glb")
    message(STATUS "Seed corpus copied to: ${CMAKE_BINARY_DIR}/corpus/")
endif()

if (GLTF_BUILD_TEST)
    # googletest
    SET(BUILD_GMOCK OFF CACHE BOOL "DISABLE GMOCK")
    SET(INSTALL_GTEST OFF CACHE BOOL "DISABLE INSTALL_GTEST")
    include(dependencies/googletest.cmake)

    add_executable(${PROJECT_NAME}_test
        test/main.cpp
        test/extensions.cpp
        test/validation.cpp
    )
    target_link_libraries(${PROJECT_NAME}_test
        ${PROJECT_NAME}
        gtest
        meshoptimizer
    )
    add_test(NAME gltf_test COMMAND ${PROJECT_NAME}_test)
    list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
endif()
