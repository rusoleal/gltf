# Changelog

## [0.4.1] - 2026-04-12
- prevent fetch vector_math if already fetched.

## [0.4.0] - 2026-04-10

### Added
- New `GLTF::loadGLTF()` overload that accepts a callback for loading external resources:
  ```cpp
  std::shared_ptr<GLTF> loadGLTF(
      const std::string &data,
      std::function<std::future<std::vector<uint8_t>>(const std::string &uri)> loadCallback);
  ```
  - The callback is invoked for each external buffer/image URI
  - Returns `std::future` to support both sync and async loading patterns
  - Automatically decompresses Draco and meshopt extensions after all resources are loaded

### Fixed
- Fixed matrix loading: GLTF stores matrices in column-major order, but `vector_math::Matrix4` uses row-major storage. `mat4FromGLTF()` now correctly transposes matrices during load.
- Updated `Node::matrix` documentation to clarify row-major storage format (`data[row*4+col]`).

### Changed
- **DEPRECATED**: `GLTF::loadGLTF(const std::string &data)` is now deprecated. Use the new callback-based overload instead.
  - Old method does not load external resources or perform automatic decompression
  - To migrate: add a callback that returns `std::future<std::vector<uint8_t>>` for each URI
- **BREAKING**: `GLTF::decompressDraco()` and `GLTF::decompressMeshopt()` are now private.
  - Decompression is automatically handled by all load methods
  - No manual decompression required when using `loadGLTF` with callback or `loadGLB`
- Minor CI workflow updates.

## [0.3.6] - 2026-04-08

- Bump `vector_math` dependency to version v0.3.5

## [0.3.5] - 2026-04-06

### Changed
- Unity build enabled (`UNITY_BUILD ON`) for both the `gltf` library and `gltf_test` targets.
- Removed duplicate `findNode` / `findMaterial` helper definitions from `test/extensions.cpp` (now shared from `test/main.cpp`).
- `findNode` / `findMaterial` in `test/main.cpp` marked `static` for correct internal linkage.

## [0.3.4] - 2026-03-23

### Fixed
- `M_PI_4` undeclared on MSVC: `_USE_MATH_DEFINES` moved from header `#define` (order-dependent) to a CMake `PUBLIC` compile definition, ensuring it is active before any header is processed in every translation unit.
- `gltf.lib` import library not generated on Windows: added `WINDOWS_EXPORT_ALL_SYMBOLS ON` so MSVC produces the import library required for linking against the shared library.
- `GLTF::scene` field never assigned in constructor despite being a constructor parameter, leaving it uninitialized (visible as garbage on MSVC debug builds).
- `loadFileAsString` opened files in text mode, corrupting binary `.glb` data on Windows due to `\r\n` → `\n` translation and causing the GLB size check to fail. Fixed by opening with `std::ios::binary`.

## [0.3.3] - 2026-03-23

### Fixed
- On MSVC, draco builds a target named `draco` instead of `draco_static`. Added a CMake alias so `target_link_libraries(... draco_static)` resolves correctly on Windows.

## [0.3.2] - 2026-03-20

### Changed
- `vector_math` changed to `PUBLIC` in `target_link_libraries` so its types (which appear in the public API) are correctly propagated to consuming targets.
- Removed redundant `$<BUILD_INTERFACE:${extern_vector_math_SOURCE_DIR}/inc>` from `target_include_directories`; the include path is now propagated transitively via the `PUBLIC` link to `vector_math`.

## [0.3.1] - 2026-03-20

### Fixed
- `draco_features.h` not found when consuming the package as a FetchContent subdependency. Draco generates this header into `${CMAKE_BINARY_DIR}` (the top-level binary directory), but `dependencies/draco.cmake` was adding `${extern_draco_BINARY_DIR}` (Draco's own nested build dir) to the include path instead. The mismatch was hidden when building gltf standalone because `PROJECT_BINARY_DIR` happened to equal `CMAKE_BINARY_DIR`. Corrected to `${CMAKE_BINARY_DIR}`.

## [0.3.0] - 2026-03-19

### Added
- CMake install and `find_package` support: exports `gltfTargets.cmake`, `gltfConfig.cmake`, `gltfConfigVersion.cmake`.
- `GLTF_REAL_NUMBER_TYPE` exposed as a CMake cache variable and propagated via `target_compile_definitions PUBLIC` to prevent ABI mismatches in consuming projects.
- `KHR_draco_mesh_compression` extension: decompression via Google Draco.
- `EXT_meshopt_compression` / `KHR_meshopt_compression` extensions: decompression via meshoptimizer.
- `KHR_animation_pointer` extension.
- `KHR_materials_variants` extension (primitive mappings and top-level variants array).
- `KHR_xmp_json_ld` extension (asset, material, mesh, and node packet indices).
- Sparse accessor parsing (`Accessor::Sparse` with `indices` and `values`).
- Morph target parsing (`Primitive::targets` as attribute-name → accessor-index maps).
- Accessor `min` / `max` parsed as `std::optional<std::vector<GLTF_REAL_NUMBER_TYPE>>`.
- Camera polymorphism: `cameras` is now `shared_ptr<vector<shared_ptr<Camera>>>`, enabling `dynamic_pointer_cast<PerspectiveCamera>` / `dynamic_pointer_cast<OrthographicCamera>`.
- `PerspectiveCamera::aspectRatio` and `zFar` are `std::optional` (both are optional per spec).
- Comprehensive validation test suite (219 tests) covering all required-field and out-of-range error paths.

### Changed
- `Primitive::targets` type changed from `vector<uint8_t>` to `vector<unordered_map<string, uint64_t>>`.
- `Accessor::min` / `max` type changed from `optional<vector<uint8_t>>` to `optional<vector<GLTF_REAL_NUMBER_TYPE>>`.
- Upgraded `vector_math` dependency to v0.3.4.
- `target_link_libraries` for internal dependencies (`draco`, `meshoptimizer`, `vector_math`) changed to `PRIVATE` to avoid leaking transitive dependencies to consumers.

### Fixed
- All required JSON fields (`buffer.byteLength`, `bufferView.buffer/byteLength`, `accessor.componentType/count/type`, `camera.type`, `perspective.yfov/znear`, `orthographic.xmag/ymag/zfar/znear`, `mesh.primitives`, `primitive.attributes`, `KHR_lights_punctual` light `type`, `EXT_meshopt_compression` `byteLength/byteStride/count`) now throw `std::invalid_argument` on missing or malformed input instead of silently producing zero or null values.
- Extension blocks for primitive, mesh, node, and material now use a consistent `emptyExtensions` fallback pattern, preventing null insertion into JSON objects.
- `clearcoatNormalTexture` missing `index` field and missing `extensions` guard.

## [0.2.1] - 2026-03-14

### Changed
- Upgraded `vector_math` dependency from v0.0.1 to v0.1.0.

## [0.2.0] - 2026-03-14

### Added
- `EXT_mesh_gpu_instancing` extension fully implemented (`Node::extMeshGpuInstancing`).
- `.glb` binary format test fixture and test cases (`AlphaBlendModeTestGLB`).
- Embedded glTF test fixture (`AlphaBlendModeTestEmbedded`) using a self-contained inline JSON — no external file dependency.
- Comprehensive unit tests (53 total) covering materials (alpha modes, cutoff values, PBR properties), nodes (names, translations, rotations), meshes (primitive attributes, mode, indices), accessors (component type validity, buffer view range), buffer views, samplers, textures, and `getRuntimeInfo()`.
- `launch_test.sh` convenience script for local test execution.
- `CLAUDE.md` guidance file for Claude Code.

### Changed
- CI pipeline now downloads all three test asset variants (`.gltf`, embedded `.gltf`, `.glb`) before running tests.

### Fixed
- Removed debug `printf` statements from `GLTF` constructor and destructor.

## [0.1.0] - 2025-06-28

### Added
- `vector_math` consumed as an independent external library via CMake `FetchContent` (removed bundled copy).
- `launch_test.sh` and CI asset download steps to run tests against KhronosGroup sample models.

### Changed
- `EXT_mesh_gpu_instancing` status updated to implemented in README.
- Test suite refactored to load assets from files using `loadFileAsString`.
