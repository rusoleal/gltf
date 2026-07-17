# glTF 2.1 Compatibility Tasks

> This list tracks the work needed to bring the loader in line with the glTF 2.1 core features announced by Khronos (June 2026). The specification is still being finalized, so tasks should be revisited as the normative text and schemas are published.

## Sources

- Khronos announcement: [Introducing glTF 2.1 with Complex Scenes](https://www.khronos.org/blog/introducing-gltf-2-1-with-complex-scenes)
- Tracking issue: [KhronosGroup/glTF#2585](https://github.com/KhronosGroup/glTF/issues/2585)
- Feature explainers:
  - External Assets: [#2586](https://github.com/KhronosGroup/glTF/issues/2586)
  - Packaging External Assets: [#2589](https://github.com/KhronosGroup/glTF/issues/2589)
  - Unified File References: [#2590](https://github.com/KhronosGroup/glTF/issues/2590)
  - Shapes: [#2588](https://github.com/KhronosGroup/glTF/issues/2588)
  - Bounding Volumes: [#2587](https://github.com/KhronosGroup/glTF/issues/2587)
  - Thumbnails: [#2591](https://github.com/KhronosGroup/glTF/issues/2591)
  - 64-bit Binary Format: [#2592](https://github.com/KhronosGroup/glTF/issues/2592)
  - Accessor Component Types: [#2596](https://github.com/KhronosGroup/glTF/issues/2596)
  - Non-Sequential Attributes: [#2593](https://github.com/KhronosGroup/glTF/issues/2593)
  - Unique IDs: [#2594](https://github.com/KhronosGroup/glTF/issues/2594)
  - Promoted Extension Functionality: [#2595](https://github.com/KhronosGroup/glTF/issues/2595)
  - Multiple Scenes Deprecation: [#2591](https://github.com/KhronosGroup/glTF/issues/2591)

## 1. Versioning & Core Parsing

- [x] Accept `"2.1"` (and any `2.x`) in `asset.version` validation (`src/gltf.cpp`).
- [x] Keep the 2.0 reject-3.0 behavior intact.
- [x] Update `implementedExtensions` so functionality promoted to core in 2.1 is recognized.
- [x] Update `README.md`/`CHANGELOG.md` once 2.1 support is implemented.

## 2. Unified File References (`files` array)

- [x] Add a top-level `File` struct and `std::shared_ptr<std::vector<File>> files` to the `GLTF` root object (`inc/gltf/file.hpp`, `inc/gltf/gltf.hpp`).
- [x] Parse the new `files` JSON array in `GLTF::loadGLTF` (`src/gltf.cpp`). Each entry has `uri`, `bufferView`, required `mimeType`, and may carry `name`/`uid`.
- [ ] Update dependency discovery (`getRuntimeInfo` and any URI-resolution helpers) to inspect `buffers`, `images`, and `files` together.
- [ ] Ensure file URIs are resolved consistently across external, embedded, and packaged resources.

## 3. External Assets & Scene Composition

- [x] Add top-level `externalAssets` array and a `Node::externalAsset` index referencing it.
- [x] Parse `externalAssets` entries (`name`, `file`) in `GLTF::loadGLTF`.
- [ ] Detect and reject cyclical external references.
- [ ] Extend the external-resource callback API to fetch nested glTF files and their dependencies.
- [ ] Support packaged/embedded external assets where nested file URIs resolve against the host `files` array.
- [ ] Support multiple instances of the same referenced asset with per-instance transforms.

## 4. Shapes

- [x] Add a top-level `shapes` array and a `Shape` base/derived type covering box, sphere, capsule, cylinder, and plane (`inc/gltf/shape.hpp`).
- [x] Parse the `shapes` JSON array in `GLTF::loadGLTF`.
- [ ] Define geometry parameterization and UV semantics matching the 2.1 explainer.

## 5. Bounding Volume Hierarchies

- [x] Add a `boundingVolume` property to `Node` (`inc/gltf/node.hpp`) referencing a `Shape` index plus an optional local transform.
- [x] Parse `node.boundingVolume` from JSON.
- [ ] Validate that bounding-volume shape indices are in range.
- [ ] Expose bounding volumes through `RuntimeInfo` or a new query API for culling/streaming consumers.

## 6. Thumbnails

- [x] Add an `asset.thumbnail` / top-level thumbnail reference to the `GLTF` root object.
- [x] Parse embedded (`bufferView`) or external (`uri`) thumbnail images.
- [ ] Provide an accessor so applications can retrieve a preview image without loading the full scene graph.

## 7. 64-bit GLB Binary Format (Version 3)

- [x] Update `GLTF::loadGLB` to parse both GLB v2 (32-bit chunk lengths) and v3 (64-bit chunk lengths).
- [x] Raise or remove the current 2 GB/4 GB assumptions (`GLTF_MAX_BUFFER_BYTE_LENGTH`, 32-bit header reads).
- [ ] Preserve v2 export/generation capability for backward compatibility.
- [x] Add unit tests with both v2 and v3 GLB fixtures.

## 8. Promoted Extension Functionality (Core in 2.1)

- [x] `KHR_node_visibility`: add `Node::visible`, parse core property and extension form, core takes precedence.
- [ ] `EXT_texture_webp`: already implemented as an extension — ensure behavior is unconditional/core-compliant.
- [ ] `KHR_materials_emissive_strength`: already implemented as an extension — ensure behavior is unconditional/core-compliant.
- [ ] `KHR_mesh_quantization`: already implemented as an extension — ensure behavior is unconditional/core-compliant.

## 9. Non-Sequential Attributes

- [x] Relax the assumption that `TEXCOORD_*` and `COLOR_*` attribute semantics start at `0` and are consecutive.
- [x] Add tests confirming `TEXCOORD_1`-only and `COLOR_2`-only meshes load.

## 10. Accessor Component Type Definitions

- [x] Extend `ComponentType` enum to include the new 2.1 type constants: `SIGNED_INT` (5124), `DOUBLE` (5130), `HALF_FLOAT` (5131), `SIGNED_INT64` (5134), `UNSIGNED_INT64` (5135).
- [ ] Update `min`/`max` parsing and accessor byte-size helpers to account for 64-bit and half-float types.

## 11. Unique IDs (UIDs)

- [x] Add a `uid` field to `GLTFChildOfRoot`.
- [x] Parse `uid` on every child-of-root object.
- [x] Enforce file-scoped uniqueness against both `uid` and `name` values during load.
- [ ] Provide a lookup API for cross-file/stable object referencing.
- [ ] Allow `name` to serve as a fallback UID when it already satisfies UID rules.

## 12. Multiple Scenes Deprecation

- [ ] Mark the multi-scene pattern as deprecated in documentation.
- [ ] Keep existing parsing support so valid 2.0 files continue to load.
- [ ] Issue a warning (or expose a diagnostic) when a loaded file contains multiple scenes.
- [ ] Do not generate multi-scene output in any exporter/serializer path.

## 13. Tests & Validation

- [x] Add `test/gltf21.cpp` with inline JSON tests for version, visibility, accessor types, non-sequential attributes, and UIDs.
- [x] Add local fixtures under `test/assets/gltf21/`.
- [x] Ensure `test/CMakeLists.txt` compiles the new test file.
- [x] Add tests for:
  - External asset reference parsing
  - Packaged/embedded external assets
  - GLB v3 64-bit chunk parsing
  - Shapes and bounding volumes
  - Thumbnail extraction
- [ ] Update `test/validation.cpp` to cover new top-level arrays (`files`, `shapes`, `externalAssets`) and node properties (`boundingVolume`, `visible`).

## 14. Official Khronos 2.1 Sample Assets

- [x] Watch the [Khronos glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets) repo for a published `2.1/` model set.
- [x] Add download targets in `launch_test.sh` (or a dedicated script) to fetch official 2.1 assets.
- [x] Add integration tests that load each official 2.1 sample and validate that the parser accepts it.
- [x] Make the downloads/tests optional so CI does not fail before Khronos publishes the assets.

## 15. Documentation

- [x] Document which 2.1 features are supported and which remain unsupported.
- [x] Update API docs to describe new load-time behavior (external asset fetching, packaged files).
- [ ] Add a migration note for users currently relying on multi-scene files.

## 16. Missing Official Extensions

Parser-level support for the following Khronos and multi-vendor extensions:

- [x] `KHR_node_hoverability`
- [x] `KHR_node_selectability`
- [x] `KHR_materials_diffuse_transmission`
- [x] `KHR_gaussian_splatting`
- [x] `EXT_texture_astc`
- [x] `EXT_lights_ies`
- [x] `EXT_lights_image_based`
- [x] `EXT_mesh_manifold`
- [x] `EXT_mesh_primitive_edge_visibility`
- [x] `EXT_mesh_primitive_restart`
- [ ] `KHR_interactivity` (deferred to a separate plan due to scope)
- [x] Add `KHR_node_visibility` to the README extensions table.
- [x] Add tests for all new extensions.
- [x] Update `CHANGELOG.md`.
