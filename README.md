# C++ glTF 2.0 helper library

A c++ package that allows loading .gltf and .glb (glTF binary format) files according to the [glTF 2.0 specification.](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)

## Public API

```cpp
// Parse a JSON .gltf string (external buffers must be filled manually afterwards).
static std::shared_ptr<GLTF> GLTF::loadGLTF(const std::string &data);

// Parse a binary .glb file given a raw byte buffer and its size in bytes.
static std::shared_ptr<GLTF> GLTF::loadGLB(uint8_t *data, uint64_t size);

// Returns which buffers and images are required to render a scene.
// Returns nullptr if sceneIndex is out of range.
std::shared_ptr<RuntimeInfo> GLTF::getRuntimeInfo(uint64_t sceneIndex);

// Decompress KHR_draco_mesh_compression primitives whose buffer data is available.
// Call this after manually filling GLTF::buffers[n].data for URI-referenced buffers.
static void GLTF::decompressDraco(std::shared_ptr<GLTF> &gltf);

// Decompress EXT_meshopt_compression buffer views whose buffer data is available.
static void GLTF::decompressMeshopt(std::shared_ptr<GLTF> &gltf);
```

### Error handling

Both `loadGLTF` and `loadGLB` throw `std::invalid_argument` on malformed or unsupported input. This includes missing required fields (e.g. `asset.version`, `TextureInfo.index`), unsupported required extensions, array size limits exceeded, and invalid cross-references detected at parse time (skin joint indices, animation channel sampler indices, sparse accessor count).

`getRuntimeInfo` also throws `std::invalid_argument` when traversing the scene graph if it encounters out-of-range indices for: scene nodes, node children, node mesh, node light (`KHR_lights_punctual`), primitive material, primitive accessors, accessor buffer views, material textures, KHR_materials_variants material and variant indices, and KHR_xmp_json_ld packet indices (asset, material, mesh, node). It also throws on cycles in the node hierarchy. It returns `nullptr` for an out-of-range scene index.

Indices that are not traversed by `getRuntimeInfo` (e.g. animation targets, skin references, camera indices) are not validated at runtime — the caller is responsible for bounds-checking those before use.

### External buffers

When loading a `.gltf` file that references external binary files by URI (not embedded `data:` URIs), the library constructs `GLTF::buffers` with empty `data` vectors. The caller is responsible for loading each file and filling `buffer.data` before calling `decompressDraco` / `decompressMeshopt` if those extensions are used.

### Thread safety

`loadGLTF` and `loadGLB` are stateless and safe to call concurrently. A single `GLTF` instance is not thread-safe for concurrent mutation; concurrent reads are safe.

## Features

* Load .gltf format with external buffers/images.
* Load .gltf format with embedded buffers/images.
* Load .glb format (glTF binary format).
* Dynamic asset info via GLTF::getRuntimeInfo() method
* Sparse accessor support (`accessor.sparse`).
* Morph targets (`primitive.targets[]`).
* Skeletal skin data (`skin.joints`, `skin.inverseBindMatrices`, `skin.skeleton`).

## Known Limitations

* **Sparse accessors** — the `Accessor::sparse` field is parsed and exposed, but the library does not automatically apply sparse overrides to the base buffer data. The caller must do this manually if needed.
* **Morph targets** — `Primitive::targets` is parsed and exposed as a vector of attribute maps (attribute name → accessor index). Computing the final morphed geometry is left to the caller.
* **Skin matrices** — joint indices and `inverseBindMatrices` are parsed and stored in `Skin`. Computing the final joint transform matrices is left to the caller.
* **Camera downcasting** — `GLTF::cameras` holds `std::shared_ptr<Camera>`. To access `PerspectiveCamera`-specific fields (`aspectRatio`, `zFar`) use `std::dynamic_pointer_cast<PerspectiveCamera>(gltf->cameras->at(i))`.
* **Input size limits** — by default the library rejects assets with more than 65 536 elements in any top-level array (buffers, bufferViews, accessors, meshes, nodes, materials) and buffers larger than 2 GB.

## Extensions

Ratified glTF 2.0 extensions:

|                                 | Status  | Reference                                                                                                      |
|---------------------------------|---------|----------------------------------------------------------------------------------------------------------------|
| KHR_animation_pointer           | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_animation_pointer)           |
| KHR_draco_mesh_compression      | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_draco_mesh_compression)      |
| KHR_lights_punctual             | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual)             |
| KHR_materials_anisotropy        | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_anisotropy)        |
| KHR_materials_clearcoat         | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_clearcoat)         |
| KHR_materials_dispersion        | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_dispersion)        |
| KHR_materials_emissive_strength | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_emissive_strength) |
| KHR_materials_ior               | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_ior)               |
| KHR_materials_iridescence       | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_iridescence)       |
| KHR_materials_sheen             | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_sheen)             |
| KHR_materials_specular          | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_specular)          |
| KHR_materials_transmission      | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_transmission)      |
| KHR_materials_unlit             | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_unlit)             |
| KHR_materials_variants          | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_variants)          |
| KHR_materials_volume            | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_volume)            |
| KHR_mesh_quantization           | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization)           |
| KHR_texture_basisu              | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu)              |
| KHR_texture_transform           | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform)           |
| KHR_xmp_json_ld                 | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_xmp_json_ld)                 |
| EXT_mesh_gpu_instancing         | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_mesh_gpu_instancing)          |
| EXT_meshopt_compression         | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_meshopt_compression)          |
| KHR_meshopt_compression         | Ok      | Same decompression logic as EXT_meshopt_compression (Khronos-ratified name)                                    |
| EXT_texture_webp                | Ok      | [Spec.](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_texture_webp)                 |

## Dependencies

* [systems::leal::vector_math](https://github.com/rusoleal/vector_math) C++ vector_math library
* [nlohmann/json](https://github.com/nlohmann/json) Json reader/writer library
* [cpp-base64](https://github.com/ReneNyffenegger/cpp-base64) Base64 encode/decode library
* [google/draco](https://github.com/google/draco) 3D mesh compression library
* [zeux/meshoptimizer](https://github.com/zeux/meshoptimizer) Mesh optimization and compression library

## Licenses

This library is licensed under MIT license.

gltf library uses the following third party libraries.

* json.hpp : Copyright (c) 2013-2025 Niels Lohmann [Link](https://github.com/nlohmann/json)
* base64 : Copyright (C) 2004-2017, 2020-2022 René Nyffenegger [Link](https://github.com/ReneNyffenegger/cpp-base64)

