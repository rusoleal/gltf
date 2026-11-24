# Changelog

## [Unreleased]

### Added
- **`GLTFWriter` (`gltf/writer.hpp`): builds an asset and writes it as GLB 2.0.**
  - Buffer views (one binary buffer, 4-byte aligned) and accessors. Helpers for float attributes, `POSITION` (with the `min` / `max` glTF requires), `COLOR_n` as normalized bytes (RGB padded to a 4-byte stride), and indices (unsigned short when they fit, else unsigned int).
  - Images stored in the buffer, samplers, textures (optionally `KHR_texture_basisu`), and core PBR materials (optionally `KHR_materials_unlit`).
  - Meshes, nodes (TRS, children, `extras`), scenes, and `extensionsUsed` / `extensionsRequired`.
  - References are checked when added. Non-finite numbers and invalid `extras` throw.
  - Tests write assets and read them back with `loadGLB`.

### Fixed
- **Images' `mimeType` wasn't read**: the parser looked for a `mime` key, so images in buffer views (GLB textures) loaded with an empty MIME type.

## [0.11.0] - 2026-11-23

### Added
- **`InteractivityRuntime::variables()` and `eventCounts()`**, for showing a graph's live state: the variables' current values, in the graph's order, and how many handlers of each event operation (event/onStart, onTick, onSelect, onHoverIn, onHoverOut, receive) have run.

## [0.10.0] - 2026-11-22

### Added
- **KHR_interactivity runtime: the asset's structure through pointer/get.** The glTF Asset Object Model's read-only pointers are answered from the asset: array sizes (`/nodes.length`, `/nodes/N/children.length`, `/nodes/N/weights.length`, `/meshes/N/primitives.length`, `/scenes/N/nodes.length`, `/skins/N/joints.length`, `/extensions/KHR_lights_punctual/lights.length` and the other top-level arrays; 0 for an array that isn't defined) and references (`/nodes/N/mesh`, `camera`, `skin`, `parent`, `children/[i]`, `/scene`, `/scenes/N/nodes/[i]`, `/meshes/N/primitives/P/material`, `/skins/N/skeleton`, `/skins/N/joints/[i]`). A reference that doesn't exist (a node without a mesh, a root's parent) isn't valid. Khronos' Flocking and PhysicsMath read them. Test `PointerGetAnswersTheAssetStructure`.
- **`math/rgbToOkLCh` and `math/rgbFromOkLCh`**, the last operations the runtime didn't run: linear BT.709 RGB to OkLCh (hue in radians) and back, with the specification's Oklab matrices, no clamping, NaN and infinity propagated. Every operation of the specification now runs; `unsupportedOperations()` only lists extension operations. Test `OkLChConvertsLinearRgbBothWays`.

## [0.9.0] - 2026-11-22

### Added
- **KHR_interactivity runtime: animations, interpolation and hover.**
  - `animation/start`, `animation/stop` and `animation/stopAt`. They follow the specification's animation state rules: playing between any two times (beyond the keyframes the animation repeats), forwards or backwards, at a speed; `done` when the end or stop time is reached; a new start replaces the one playing without its `done`. The runtime decides the time to show and calls the new `InteractivityHost::applyAnimation(animation, time)` on each tick; `animationStopped(animation)` says the graph let go of it. The read-only Animation State pointers (`/animations/N/extensions/KHR_interactivity/isPlaying`, `minTime`, `maxTime`, `playhead`, `virtualPlayhead`) are answered by the runtime.
  - `variable/interpolate` and `pointer/interpolate`. The value eases along the cubic Bézier through `p1` and `p2`, and `done` fires on the tick that reaches `duration`. Quaternions are interpolated spherically: rotation properties, and variables with `useSlerp`. A new interpolation of the same variable or property replaces the old one, and `variable/set` or `pointer/set` ends it, in both cases without `done`. `pointer/interpolate` goes to `err` for a property that can't be read or written.
  - KHR_node_hoverability's `event/onHoverIn` and `event/onHoverOut`. The application reports the node under each pointer with the new `InteractivityRuntime::hover(node, controllerIndex)`. Hover out fires first, then hover in, each from the hovered node up to just below the lowest common ancestor of the old and new nodes; `event/stopPropagation` ends it. `usesHover()` says whether the graph has hover handlers.
  - pointer/get of a glTF object itself, as a `ref` (`/animations/[index]`, `/nodes/3`): Needle's Ghost reads its animations this way.
  - `/extensions/KHR_interactivity/limits/maxActive{Delays,Animations,PropertyInterpolations,VariableInterpolations}` (1024 each).
  - `animating()`: whether an animation or interpolation is in progress, so the application keeps drawing frames.
  - Each `tick()` advances animations, then interpolations, then delays, then runs event/onTick.
  - Tests: `GhostPlaysItsAnimationsOnHover` (the Ghost showcase model end to end), `VariableInterpolateEasesToTheTargetThenFiresDone`, `PointerInterpolateSlerpsRotations`, `AnimationStartPlaysToItsEndTimeAndWraps`, `AnimationStopAtEndsEarlyWithItsOwnDone`, `HoverInAndOutStopBelowTheCommonAncestor`.

### Changed
- **The runtime is about 4x faster.** Needle's MiniChess engine thinking took 54 ms per tick natively and now takes 14 ms. Pure node outputs are kept in a flat list per node, invalidated by a generation count instead of clearing a hash map on every step; dependency traversal reuses its storage; inline input values, variable indices and flow output names are prepared once; values are read by reference; and operation names are compared length first, with literal lengths known at compile time.

## [0.8.0] - 2026-11-21

### Added
- **KHR_interactivity runtime** (`gltf/extensions/khr_interactivity_runtime.hpp`): `InteractivityRuntime` executes an asset's default behavior graph against an `InteractivityHost` the application implements (`getPointer`/`setPointer` on the live glTF Asset Object Model, `log`, `eventSent`, `random`). The application calls `start()` (event/onStart), `tick(seconds)` (event/onTick, due flow/setDelay activations, custom events the graph sent itself) and `select(node)` (KHR_node_selectability event/onSelect, propagated to ancestors until stopped); `selectable(node)` says whether a node has a handler. Implemented: all flow operations (sequence, branch, switch, while, for, doN, multiGate, waitAll, throttle, setDelay, cancelDelay), variables, pointer/get and pointer/set with JSON Pointer templates (int `[i]` and ref `{r}` parameters; object refs substitute their index), event/send/receive and stopPropagation, debug/log templates, and the math, vector, matrix (inverse, matCompose/matDecompose, matMul, transpose, determinant) and quaternion operations, plus type conversions, with the specification's int (wrapping, division by zero gives 0) and float (NaN propagation, round half away from zero) rules. Pure nodes are evaluated on demand and cached until the next flow node runs. Not implemented yet, run as the specification's no-ops and listed in `unsupportedOperations()`: animation/*, pointer/interpolate, variable/interpolate, hover events, OkLCh colour conversion. Each event has a step budget (endless loops stop with an entry in `errors()`); value chains and flow chains are evaluated without recursion, so long graphs don't exhaust the stack. Tests `InteractivityRuntime.*`: handwritten graphs, Calculator.glb end to end (buttons update the display's texture_transform offsets and the minus sign's visibility), and every valid asset of the Khronos interactivity test set started, ticked and selected without errors.

## [0.7.1] - 2026-11-21

### Fixed
- **WebAssembly: errors in a file aborted instead of being caught.** The library reports a file's errors by throwing, and KHR_interactivity validation throws and catches internally to reject a graph, but Emscripten builds have exception catching off by default: the throw aborted the thread. A model with an invalid interactivity graph (Needle's older Calculator.glb, exported against a draft with an empty `events` array) didn't open in campello_web_viewer, which waited forever. The option `GLTF_WASM_EXCEPTIONS` (on) compiles the library with catching (Emscripten's JavaScript-based exceptions, `-fexceptions`, which work with ASYNCIFY) and links its consumers with it; code that catches the library's exceptions is compiled with `-fexceptions` too.

## [0.7.0] - 2026-11-20

### Changed
- **WebAssembly builds use SIMD** (`-msimd128`), on by default: every browser with WebGPU supports it. The option `GLTF_WASM_SIMD` turns it off. It applies to the library and the Draco and meshoptimizer decoders it builds (meshoptimizer's decoder has a SIMD path) and is passed on to consumers (compile and link, so wasm-opt accepts the SIMD instructions).
- vector_math v0.7.0 (WebAssembly SIMD).

## [0.6.0] - 2026-11-19

### Added
- **KHR_interactivity** (ratified). Behavior graphs are parsed into `GLTF::khrInteractivity` (`nullptr` when absent): the default `graph` index and each graph's `types`, `variables`, custom `events`, operation `declarations` and `nodes`, with their value sockets (inline literal or link to an earlier node's output), flow sockets (target socket defaults to `"in"`) and `configuration`. Literals keep their JSON kind (bool, integer, number, string), so `pointer` strings and exact integers survive. Validation follows the spec's Validation section without failing the glTF load: a broken structural assert sets `khrInteractivity->valid = false` (with `error`), and a "reject the graph" rule sets that graph's `valid = false`. Checked: JSON indices, non-empty arrays/objects, type signatures (known, case-sensitive, no duplicates), inline values against their type (length, bool/int32/number, JSON Pointer refs), duplicate event ids and declarations, value links to earlier nodes and flows to later ones, and every node against its operation: a built-in table of all operations of the spec (input/output value sockets and type overloads, e.g. `math/add` on floatN/floatNxN/int) plus the configuration-driven ones (`math/select`, `math/switch`, `variable/*`, `pointer/*` with JSON Pointer template parsing, `event/receive`/`send`, `debug/log` templates); extension operations use their declared sockets. Unknown operations, missing or mistyped inputs, and links to outputs that do not exist or have another type reject the graph. Running the graph is left to the caller. All 187 invalid cases of the Khronos interactivity test set are rejected (each for the rule it targets) and all 197 valid assets load. New header `gltf/extensions/khr_interactivity.hpp`, tests `KHRInteractivityTest.*`, and `KHRInteractivityConformance.*` over KhronosGroup/glTF-Test-Assets-Interactivity (cloned by `launch_test.sh` into `build/interactivity-assets`, skipped when absent).

## [0.5.3] - 2026-11-19

### Fixed
- **KHR_texture_procedurals graphs or nodes without `inputs`, `outputs` or `nodes`** (a graph with no parameters, a node left at its defaults). Those keys are optional, but they were read with a const JSON object's `operator[]`, which asserts on a missing key in debug builds and is undefined behaviour in release. Loading such a glTF aborted. They're read through a helper that gives null when absent. New test `KHRTextureProceduralsOptionalKeys.GraphsWithoutInputsParse` (aborts without the fix). Found baking procedural materials in campello_renderer.

## [0.5.2] - 2026-11-10

### Changed
- vector_math v0.6.1 (Emscripten builds).

### Fixed
- **Emscripten builds.** draco built its JavaScript decoder modules instead of its C++ libraries under Emscripten (so `draco_static` didn't exist), and its configure step required an `EMSCRIPTEN` environment variable. `dependencies/draco.cmake` now turns `DRACO_JS_GLUE` off and sets the variable from the toolchain's `EMSCRIPTEN_ROOT_PATH`, so a plain `emcmake cmake` works. Found building campello_renderer for WebGPU.

## [0.5.1] - 2026-08-19

### Changed
- Upgraded `vector_math` dependency from v0.3.5 to v0.6.0 (SIMD/AVX performance improvements and a `Matrix4::lookAt()` right-axis fix). No API usage in this library is affected, since `lookAt()` is not used internally.

### Fixed
- Removed the stale `AlphaBlendModeTestEmbedded.gltf` download from `launch_test.sh` and `CLAUDE.md`; the corresponding test uses an inline embedded glTF fixture and never reads that file. The download also pointed at a `glTF-Embedded` asset variant that KhronosGroup has since removed upstream.

## [0.5.0] - 2026-08-02

### Added
- glTF 2.1 parser support:
  - Accepts `asset.version` 2.1.
  - Promoted `KHR_node_visibility` to core `Node::visible`, with `KHR_node_visibility` extension fallback for 2.0 assets.
  - New accessor component types: `INT` (5124), `HALF_FLOAT` (5130), `DOUBLE` (5131), `UNSIGNED_BYTE_NORMALIZED` (5134), `BYTE_NORMALIZED` (5135).
  - Non-sequential `TEXCOORD_n` / `COLOR_n` primitive attributes.
  - File-scoped Unique IDs (`uid`) on every child-of-root object, with uniqueness validation against `name`.
  - Unified File References: top-level `files` array (`File` struct with `uri`, `bufferView`, `mimeType`, `data`).
  - External Assets and Packaging: top-level `externalAssets` array and `Node::externalAsset`.
  - Shape definitions: top-level `shapes` array (`BoxShape`, `SphereShape`, `CapsuleShape`, `CylinderShape`, `PlaneShape`).
  - Bounding Volumes: `Node::boundingVolume` referencing a shape index and optional matrix.
  - Thumbnails: `asset.thumbnail` parsed into `GLTF::thumbnail`.
  - GLB v3 64-bit container support, while retaining full GLB v2 compatibility.
- `test/gltf21.cpp` test suite (35 tests) covering the new 2.1 data model and validation paths.
- Additional 2.1 reference-validation tests in `test/validation.cpp`.
- Optional official Khronos glTF 2.1 sample-asset download path via `GLTF21_SAMPLE_REPO` in `launch_test.sh`.
- Parser-level support for additional official Khronos and multi-vendor extensions:
  - `KHR_node_hoverability` / `KHR_node_selectability` — parsed on `Node`.
  - `KHR_materials_diffuse_transmission` — parsed on `Material`.
  - `KHR_gaussian_splatting` — parsed on `Node` / `Mesh`.
  - `EXT_texture_astc` — parsed inside `TextureInfo`.
  - `EXT_lights_ies` — root `lights` array and per-node light reference.
  - `EXT_lights_image_based` — root `lights` array and per-scene light reference.
  - `EXT_mesh_manifold` — parsed on `Mesh` with `manifoldPrimitive` and merge accessors.
  - `EXT_mesh_primitive_edge_visibility` — parsed on primitive.
  - `EXT_mesh_primitive_restart` — parsed on primitive.
- Cross-reference validation for the new extensions: IES light indices, image-based scene light indices, restart-group primitive indices, and manifold merge accessor indices.
- Unit tests for all of the above extensions in `test/extensions.cpp` and validation tests in `test/validation.cpp`.
- Fuzz build now works out of the box on macOS with a Homebrew LLVM clang; `build-fuzz` ignored.

### Changed
- `GLTF_MAX_BUFFER_BYTE_LENGTH` raised to `0x7FFFFFFFFFFFFFFFULL` to accommodate GLB v3 64-bit buffers.
- Updated the input-size-limit validation test for the new 64-bit buffer limit.
- Upgraded `meshoptimizer` dependency from v0.22 to v1.2 for faster `EXT_meshopt_compression`/`KHR_meshopt_compression` decompression and upstream bug fixes. Decode API is backward compatible, so no loader code changes were required.

### Fixed
- `GLTF_BUILD_FUZZ` compiler check no longer fails on clang versions that support `-fsanitize=fuzzer-no-link`; the check now verifies compilation only, because the coverage symbols are resolved by the separately-built libFuzzer runtime.
- `dependencies/libfuzzer.cmake` source list updated to match compiler-rt 18.1.8 upstream (removed obsolete `FuzzerShmemPosix.cpp` / `FuzzerShmemWindows.cpp`, added missing extra-counters and platform-utility sources).

## [0.4.2] - 2026-04-28

### Added
- `KHR_texture_procedurals` extension support.
  - Parses root-level `procedurals` (procedural graphs) and `procedural_definitions` (node definitions).
  - Parses `KHR_texture_procedurals` inside any `TextureInfo` (e.g. `baseColorTexture`), with `index` and `output` fields.
  - Full data model: `ProceduralGraph`, `ProceduralNode`, `ProceduralNodeDef`, `ProceduralNodeInput`, `ProceduralNodeOutput`, `ProceduralValue`.

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
