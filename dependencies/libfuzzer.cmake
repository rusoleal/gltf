cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

# ─────────────────────────────────────────────────────────────────────────────
# Build the libFuzzer runtime from source (LLVM compiler-rt).
#
# This removes the dependency on libFuzzer being pre-installed on the system.
# A clang compiler is still required for the -fsanitize=fuzzer-no-link
# instrumentation flag (coverage hooks injected into the code under test).
#
# After this file is included, the target `fuzzer_runtime` is available.
# Fuzz targets should:
#   1. Compile with -fsanitize=fuzzer-no-link,address
#   2. Link against: ${PROJECT_NAME}  fuzzer_runtime
#   3. Link with:    -fsanitize=address  (ASan runtime only, no fuzzer runtime)
# ─────────────────────────────────────────────────────────────────────────────

include(FetchContent)

# compiler-rt tarball is ~14 MB — far smaller than the full LLVM monorepo.
# To pin to an exact binary, add:
#   URL_HASH SHA256=<hash of compiler-rt-18.1.8.src.tar.xz>
FetchContent_Declare(
    compiler_rt
    URL      https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/compiler-rt-18.1.8.src.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(compiler_rt)
if (NOT compiler_rt_POPULATED)
    message(STATUS "Fetching compiler-rt (libFuzzer source ~14 MB)...")
    FetchContent_Populate(compiler_rt)
endif()

set(_FUZZER_SRC "${compiler_rt_SOURCE_DIR}/lib/fuzzer")

# ── Sources common to all platforms ──────────────────────────────────────────
set(LIBFUZZER_SOURCES
    ${_FUZZER_SRC}/FuzzerCrossOver.cpp
    ${_FUZZER_SRC}/FuzzerDataFlowTrace.cpp
    ${_FUZZER_SRC}/FuzzerDriver.cpp
    ${_FUZZER_SRC}/FuzzerFork.cpp
    ${_FUZZER_SRC}/FuzzerIO.cpp
    ${_FUZZER_SRC}/FuzzerLoop.cpp
    ${_FUZZER_SRC}/FuzzerMain.cpp
    ${_FUZZER_SRC}/FuzzerMerge.cpp
    ${_FUZZER_SRC}/FuzzerMutate.cpp
    ${_FUZZER_SRC}/FuzzerSHA1.cpp
    ${_FUZZER_SRC}/FuzzerTracePC.cpp
    ${_FUZZER_SRC}/FuzzerUtil.cpp
)

# ── Platform-specific sources ─────────────────────────────────────────────────
if (WIN32)
    list(APPEND LIBFUZZER_SOURCES
        ${_FUZZER_SRC}/FuzzerExtFunctionsWindows.cpp
        ${_FUZZER_SRC}/FuzzerIOWindows.cpp
        ${_FUZZER_SRC}/FuzzerShmemWindows.cpp
        ${_FUZZER_SRC}/FuzzerUtilWindows.cpp
    )
elseif (APPLE)
    list(APPEND LIBFUZZER_SOURCES
        ${_FUZZER_SRC}/FuzzerExtFunctionsDlsym.cpp
        ${_FUZZER_SRC}/FuzzerIOPosix.cpp
        ${_FUZZER_SRC}/FuzzerShmemPosix.cpp
        ${_FUZZER_SRC}/FuzzerUtilDarwin.cpp
        ${_FUZZER_SRC}/FuzzerUtilPosix.cpp
    )
else()  # Linux / other POSIX
    list(APPEND LIBFUZZER_SOURCES
        ${_FUZZER_SRC}/FuzzerExtFunctionsWeak.cpp
        ${_FUZZER_SRC}/FuzzerIOPosix.cpp
        ${_FUZZER_SRC}/FuzzerShmemPosix.cpp
        ${_FUZZER_SRC}/FuzzerUtilLinux.cpp
        ${_FUZZER_SRC}/FuzzerUtilPosix.cpp
    )
endif()

add_library(fuzzer_runtime STATIC ${LIBFUZZER_SOURCES})

# The runtime itself must NEVER be instrumented or sanitized — only the code
# under test should be. Suppress all upstream warnings as well.
if (WIN32)
    target_compile_options(fuzzer_runtime PRIVATE /W0)
else()
    target_compile_options(fuzzer_runtime PRIVATE
        -w
        -fno-sanitize=all
    )
endif()

target_compile_features(fuzzer_runtime PRIVATE cxx_std_17)

# Link pthread on POSIX; dl on Linux (needed by FuzzerExtFunctionsDlsym /
# FuzzerExtFunctionsWeak for runtime symbol resolution).
if (UNIX)
    target_link_libraries(fuzzer_runtime PUBLIC pthread)
    if (NOT APPLE)
        target_link_libraries(fuzzer_runtime PUBLIC dl)
    endif()
endif()
