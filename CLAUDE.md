# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

C++ glTF 2.0 helper library for loading and parsing glTF 3D asset files (`.gltf` and `.glb` binary format). Namespace: `systems::leal::gltf`. C++17, outputs a shared library.

## Build Commands

```bash
# Build (no tests)
cmake -B build -S . && make -C build

# Build with tests
cmake -B build -S . -DGLTF_BUILD_TEST=ON && make -C build

# Optional: configure floating-point precision (default: double)
cmake -B build -S . -DGLTF_REAL_NUMBER_TYPE=float
```

## Running Tests

Tests require downloading sample glTF assets from KhronosGroup before running:

```bash
# Use the provided script (handles build + asset download + run)
./launch_test.sh

# Or manually:
cmake -B build -S . -DGLTF_BUILD_TEST=ON && make -C build
curl https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf > build/AlphaBlendModeTest.gltf
curl https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF-Embedded/AlphaBlendModeTestEmbedded.gltf > build/AlphaBlendModeTestEmbedded.gltf
curl https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF-Binary/AlphaBlendModeTest.glb > build/AlphaBlendModeTest.glb
cd build && ./gltf_test

# Run a single test suite
cd build && ./gltf_test --gtest_filter=AlphaBlendModeTest.*
cd build && ./gltf_test --gtest_filter=AlphaBlendModeTestEmbedded.*
cd build && ./gltf_test --gtest_filter=AlphaBlendModeTestGLB.*
```

## Architecture

### Public API (`inc/gltf/gltf.hpp`)
- `GLTF::loadGLTF(const std::string &data, callback)` — parse JSON `.gltf` string with external resource loading
- `GLTF::loadGLB(uint8_t *data, uint64_t size)` — parse binary `.glb` data
- `GLTF::getRuntimeInfo(uint64_t sceneIndex)` — get scene runtime metadata

The callback for `loadGLTF` receives a URI and returns `std::future<std::vector<uint8_t>>`:
```cpp
auto gltf = GLTF::loadGLTF(json, [](const std::string& uri) {
    return std::async(std::launch::deferred, [&uri]() {
        return loadFile(uri);  // your file loading function
    });
});
```

Both `loadGLTF` and `loadGLB` automatically decompress Draco and meshopt extensions.

### Data Flow
```
.gltf JSON / .glb binary
    → gltf.cpp (parser, uses nlohmann/json embedded in src/json.hpp)
    → base64.cpp (decodes embedded buffers/images)
    → utils.cpp (JSON↔C++ type conversions)
    → Typed C++ objects (GLTF root class with shared_ptr vectors of components)
```

### Component Hierarchy
The `GLTF` root object owns shared_ptr vectors of all glTF components:

- **Buffer / BufferView / Accessor** — binary geometry data pipeline; Accessor provides typed access (scalars, vectors, matrices)
- **Image / Texture / Sampler** — texture data pipeline
- **Material** — PBR material with texture references and extensions
- **Mesh / Primitive** — geometry; Primitive holds vertex attributes, index buffer, and material reference
- **Node / Scene** — scene graph; Node holds TRS transforms and references to Mesh/Camera/Skin
- **Animation** — keyframe data with linear/step/cubic interpolation
- **Skin / Camera** — skeletal and camera data

Extensions live in `inc/gltf/extensions/`: `KHR_lights_punctual` and `EXT_mesh_gpu_instancing`.

### Dependencies
- **vector_math** (fetched via CMake FetchContent) — Vector2/3/4, Quaternion, Matrix types
- **nlohmann/json** — embedded in `src/json.hpp`
- **cpp-base64** — embedded in `src/base64.cpp`
- **GoogleTest** — fetched at configure time when `GLTF_BUILD_TEST=ON`
