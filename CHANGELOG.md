# Changelog

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
