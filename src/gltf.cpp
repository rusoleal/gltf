#define _USE_MATH_DEFINES // windows specific for M_PI constants
#include <cmath>

#include <exception>
#include <future>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <gltf/gltf.hpp>
#include "json.hpp"
#include "base64.h"
#include "utils.hpp"
#include "gltf_config.h"
#include <draco/compression/decode.h>
#include <meshoptimizer.h>

using namespace systems::leal::gltf;
using json = nlohmann::json;

static constexpr uint64_t GLTF_MAX_ARRAY_ELEMENTS  = 65536;
// glTF 2.1's 64-bit GLB format raises the previous 2 GB limit to 8 EiB.
static constexpr uint64_t GLTF_MAX_BUFFER_BYTE_LENGTH = 0x7FFFFFFFFFFFFFFFULL;

/**
 * Parse the optional glTF 2.1 unique identifier from a child-of-root object.
 * Returns an empty string when the field is absent or not a string.
 */
static std::string parseUid(const nlohmann::json &objDef)
{
    if (objDef.contains("uid") && objDef.at("uid").is_string())
    {
        return objDef.at("uid").get<std::string>();
    }
    return "";
}

/**
 * Validate glTF 2.1 unique identifiers across all child-of-root objects.
 * A UID must be unique within the file and must not collide with any name.
 */
static void validateUids(
    const std::vector<Buffer> &buffers,
    const std::vector<BufferView> &bufferViews,
    const std::vector<Accessor> &accessors,
    const std::vector<std::shared_ptr<Camera>> &cameras,
    const std::vector<Image> &images,
    const std::vector<Texture> &textures,
    const std::vector<Scene> &scenes,
    const std::vector<Node> &nodes,
    const std::vector<Sampler> &samplers,
    const std::vector<Mesh> &meshes,
    const std::vector<Material> &materials,
    const std::vector<Animation> &animations,
    const std::vector<Skin> &skins,
    const std::vector<KHRLightPunctual> &khrLightsPunctual,
    const std::vector<EXTLightIESProfile> &extLightsIes,
    const std::vector<EXTLightImageBased> &extLightsImageBased,
    const std::vector<File> &files,
    const std::vector<std::shared_ptr<Shape>> &shapes,
    const std::vector<ExternalAsset> &externalAssets)
{
    std::unordered_set<std::string> names;
    std::unordered_set<std::string> uids;

    auto reserveName = [&names](const std::string &name) {
        if (!name.empty()) names.insert(name);
    };

    auto checkUid = [&names, &uids](const std::string &uid) {
        if (uid.empty()) return;
        if (!uids.insert(uid).second)
        {
            throw std::invalid_argument("Duplicate uid: " + uid);
        }
        if (names.count(uid))
        {
            throw std::invalid_argument("uid collides with name: " + uid);
        }
    };

    // Collect names first so a later uid cannot collide with any earlier name.
    for (const auto &v : buffers)        reserveName(v.name);
    for (const auto &v : bufferViews)    reserveName(v.name);
    for (const auto &v : accessors)      reserveName(v.name);
    for (const auto &v : cameras)        if (v) reserveName(v->name);
    for (const auto &v : images)         reserveName(v.name);
    for (const auto &v : textures)       reserveName(v.name);
    for (const auto &v : scenes)         reserveName(v.name);
    for (const auto &v : nodes)          reserveName(v.name);
    for (const auto &v : samplers)       reserveName(v.name);
    for (const auto &v : meshes)         reserveName(v.name);
    for (const auto &v : materials)      reserveName(v.name);
    for (const auto &v : animations)     reserveName(v.name);
    for (const auto &v : skins)          reserveName(v.name);
    for (const auto &v : khrLightsPunctual) reserveName(v.name);
    for (const auto &v : extLightsIes)   reserveName(v.name);
    for (const auto &v : extLightsImageBased) reserveName(v.name);
    for (const auto &v : files)          reserveName(v.name);
    for (const auto &v : shapes)         if (v) reserveName(v->name);
    for (const auto &v : externalAssets) reserveName(v.name);

    // Now validate every explicit uid.
    for (const auto &v : buffers)        checkUid(v.uid);
    for (const auto &v : bufferViews)    checkUid(v.uid);
    for (const auto &v : accessors)      checkUid(v.uid);
    for (const auto &v : cameras)        if (v) checkUid(v->uid);
    for (const auto &v : images)         checkUid(v.uid);
    for (const auto &v : textures)       checkUid(v.uid);
    for (const auto &v : scenes)         checkUid(v.uid);
    for (const auto &v : nodes)          checkUid(v.uid);
    for (const auto &v : samplers)       checkUid(v.uid);
    for (const auto &v : meshes)         checkUid(v.uid);
    for (const auto &v : materials)      checkUid(v.uid);
    for (const auto &v : animations)     checkUid(v.uid);
    for (const auto &v : skins)          checkUid(v.uid);
    for (const auto &v : khrLightsPunctual) checkUid(v.uid);
    for (const auto &v : extLightsIes)   checkUid(v.uid);
    for (const auto &v : extLightsImageBased) checkUid(v.uid);
    for (const auto &v : files)          checkUid(v.uid);
    for (const auto &v : shapes)         if (v) checkUid(v->uid);
    for (const auto &v : externalAssets) checkUid(v.uid);
}


const std::array<char const *, 35> GLTF::implementedExtensions = {
    "KHR_lights_punctual",
    "EXT_texture_webp",
    "EXT_mesh_gpu_instancing",
    "KHR_mesh_quantization",
    "KHR_materials_unlit",
    "KHR_materials_emissive_strength",
    "KHR_materials_ior",
    "KHR_materials_dispersion",
    "KHR_texture_basisu",
    "KHR_texture_transform",
    "KHR_materials_transmission",
    "KHR_materials_clearcoat",
    "KHR_materials_sheen",
    "KHR_materials_specular",
    "KHR_materials_volume",
    "KHR_materials_iridescence",
    "KHR_materials_anisotropy",
    "KHR_materials_variants",
    "KHR_xmp_json_ld",
    "KHR_animation_pointer",
    "KHR_draco_mesh_compression",
    "EXT_meshopt_compression",
    "KHR_meshopt_compression",
    "KHR_texture_procedurals",
    "KHR_node_visibility",
    "KHR_node_hoverability",
    "KHR_node_selectability",
    "KHR_materials_diffuse_transmission",
    "KHR_gaussian_splatting",
    "EXT_texture_astc",
    "EXT_lights_ies",
    "EXT_lights_image_based",
    "EXT_mesh_manifold",
    "EXT_mesh_primitive_edge_visibility",
    "EXT_mesh_primitive_restart"
};

GLTF::GLTF(
    std::shared_ptr<std::vector<Buffer>> buffers,
    std::shared_ptr<std::vector<BufferView>> bufferViews,
    std::shared_ptr<std::vector<Accessor>> accessors,
    std::shared_ptr<std::vector<std::shared_ptr<Camera>>> cameras,
    std::shared_ptr<std::vector<Image>> images,
    std::shared_ptr<std::vector<Texture>> textures,
    int64_t scene,
    std::shared_ptr<std::vector<Scene>> scenes,
    std::shared_ptr<std::vector<Node>> nodes,
    std::shared_ptr<std::vector<Sampler>> samplers,
    std::shared_ptr<std::vector<Mesh>> meshes,
    std::shared_ptr<std::vector<Material>> materials,
    std::shared_ptr<std::vector<Animation>> animations,
    std::shared_ptr<std::vector<Skin>> skins,
    std::shared_ptr<std::vector<KHRLightPunctual>> khrLightsPunctual,
    std::shared_ptr<KHRTextureProcedurals> khrTextureProcedurals,
    std::shared_ptr<std::vector<EXTLightIESProfile>> extLightsIes,
    std::shared_ptr<std::vector<EXTLightImageBased>> extLightsImageBased)
{
    this->buffers = buffers;
    this->bufferViews = bufferViews;
    this->accessors = accessors;
    this->cameras = cameras;
    this->images = images;
    this->textures = textures;
    this->scene = scene;
    this->scenes = scenes;
    this->nodes = nodes;
    this->samplers = samplers;
    this->meshes = meshes;
    this->materials = materials;
    this->animations = animations;
    this->skins = skins;
    this->khrLightsPunctual = khrLightsPunctual;
    this->khrTextureProcedurals = khrTextureProcedurals;
    this->extLightsIes = extLightsIes;
    this->extLightsImageBased = extLightsImageBased;
}

GLTF::~GLTF()
{
}

void GLTF::updateRuntimeInfoWithNode(std::shared_ptr<RuntimeInfo> runtimeInfo, Node &node, std::unordered_set<uint64_t> &visited)
{
    if (node.mesh != -1)
    {
        if (static_cast<size_t>(node.mesh) >= meshes->size())
        {
            throw std::invalid_argument("Node references out-of-range mesh index " + std::to_string(node.mesh));
        }
        auto &mesh = (*meshes)[node.mesh];

        // KHR_xmp_json_ld (mesh)
        if (mesh.khrXmpPacket != -1)
        {
            if (khrXmpPackets == nullptr || static_cast<size_t>(mesh.khrXmpPacket) >= khrXmpPackets->size())
                throw std::invalid_argument("Mesh references out-of-range XMP packet index " + std::to_string(mesh.khrXmpPacket));
        }

        for (auto &primitive : mesh.primitives)
        {
            if (primitive.material != -1)
            {
                if (static_cast<size_t>(primitive.material) >= materials->size())
                {
                    throw std::invalid_argument("Primitive references out-of-range material index " + std::to_string(primitive.material));
                }
                updateRuntimeInfoWithMaterial(runtimeInfo, (*materials)[primitive.material]);
            }

            // KHR_materials_variants mappings
            for (auto &mapping : primitive.khrMaterialsVariantsMappings)
            {
                if (mapping.material != -1 && static_cast<size_t>(mapping.material) >= materials->size())
                    throw std::invalid_argument("KHR_materials_variants mapping references out-of-range material index " + std::to_string(mapping.material));
                for (auto variantIdx : mapping.variants)
                {
                    if (khrMaterialsVariants == nullptr || variantIdx >= khrMaterialsVariants->size())
                        throw std::invalid_argument("KHR_materials_variants mapping references out-of-range variant index " + std::to_string(variantIdx));
                }
            }

            // search for indices buffer
            if (primitive.indices != -1)
            {
                if (static_cast<size_t>(primitive.indices) >= accessors->size())
                {
                    throw std::invalid_argument("Primitive references out-of-range accessor index " + std::to_string(primitive.indices));
                }
                auto &accessor = (*accessors)[primitive.indices];
                if (accessor.bufferView != -1)
                {
                    if (static_cast<size_t>(accessor.bufferView) >= bufferViews->size())
                    {
                        throw std::invalid_argument("Accessor references out-of-range bufferView index " + std::to_string(accessor.bufferView));
                    }
                    auto &bufferView = (*bufferViews)[accessor.bufferView];
                    runtimeInfo->buffers[bufferView.buffer] = true;
                }
            }

            // search for vertex attributes
            for (auto const &[key, val] : primitive.attributes)
            {
                if (val >= accessors->size())
                {
                    throw std::invalid_argument("Primitive attribute '" + key + "' references out-of-range accessor index " + std::to_string(val));
                }
                auto accessor = (*accessors)[val];
                if (accessor.bufferView != -1)
                {
                    if (static_cast<size_t>(accessor.bufferView) >= bufferViews->size())
                    {
                        throw std::invalid_argument("Accessor references out-of-range bufferView index " + std::to_string(accessor.bufferView));
                    }
                    auto &bufferView = (*bufferViews)[accessor.bufferView];
                    runtimeInfo->buffers[bufferView.buffer] = true;
                }
            }
        }
    }

    // KHR_lights_punctual
    if (node.light != -1 && static_cast<size_t>(node.light) >= khrLightsPunctual->size())
        throw std::invalid_argument("Node references out-of-range light index " + std::to_string(node.light));

    // EXT_lights_ies
    if (node.extLightsIes != nullptr)
    {
        if (node.extLightsIes->light < 0 || static_cast<size_t>(node.extLightsIes->light) >= extLightsIes->size())
            throw std::invalid_argument("Node references out-of-range IES light index " + std::to_string(node.extLightsIes->light));
    }

    // KHR_xmp_json_ld (node)
    if (node.khrXmpPacket != -1)
    {
        if (khrXmpPackets == nullptr || static_cast<size_t>(node.khrXmpPacket) >= khrXmpPackets->size())
            throw std::invalid_argument("Node references out-of-range XMP packet index " + std::to_string(node.khrXmpPacket));
    }

    for (auto child : node.children)
    {
        if (child >= nodes->size())
        {
            throw std::invalid_argument("Node references out-of-range child index " + std::to_string(child));
        }
        if (visited.count(child))
        {
            throw std::invalid_argument("Cycle detected in node hierarchy at index " + std::to_string(child));
        }
        visited.insert(child);
        updateRuntimeInfoWithNode(runtimeInfo, (*nodes)[child], visited);
    }
}

void GLTF::updateRuntimeInfoWithMaterial(std::shared_ptr<RuntimeInfo> runtimeInfo, Material &material)
{

    auto resolveTexture = [&](int64_t index) {
        if (index < 0 || static_cast<size_t>(index) >= textures->size())
        {
            throw std::invalid_argument("Material references out-of-range texture index " + std::to_string(index));
        }
        auto &texture = (*textures)[index];
        if (texture.source != -1)
        {
            runtimeInfo->images[texture.source] = true;
        }
    };

    // KHR_xmp_json_ld (material)
    if (material.khrXmpPacket != -1)
    {
        if (khrXmpPackets == nullptr || static_cast<size_t>(material.khrXmpPacket) >= khrXmpPackets->size())
            throw std::invalid_argument("Material references out-of-range XMP packet index " + std::to_string(material.khrXmpPacket));
    }

    if (material.emissiveTexture != nullptr)
    {
        resolveTexture(material.emissiveTexture->index);
    }

    if (material.normalTexture != nullptr)
    {
        resolveTexture(material.normalTexture->index);
    }

    if (material.occlusionTexture != nullptr)
    {
        resolveTexture(material.occlusionTexture->index);
    }

    if (material.pbrMetallicRoughness != nullptr)
    {
        if (material.pbrMetallicRoughness->baseColorTexture != nullptr)
        {
            resolveTexture(material.pbrMetallicRoughness->baseColorTexture->index);
        }

        if (material.pbrMetallicRoughness->metallicRoughnessTexture != nullptr)
        {
            resolveTexture(material.pbrMetallicRoughness->metallicRoughnessTexture->index);
        }
    }
}

std::shared_ptr<RuntimeInfo> GLTF::getRuntimeInfo(uint64_t sceneIndex)
{
    if (sceneIndex >= scenes->size())
    {
        return nullptr;
    }
    auto toReturn = std::make_shared<RuntimeInfo>(buffers->size(), images->size());

    // KHR_xmp_json_ld (asset-level packet)
    if (khrXmpAssetPacket != -1)
    {
        if (khrXmpPackets == nullptr || static_cast<size_t>(khrXmpAssetPacket) >= khrXmpPackets->size())
            throw std::invalid_argument("Asset references out-of-range XMP packet index " + std::to_string(khrXmpAssetPacket));
    }

    auto &scene = (*scenes)[sceneIndex];

    // EXT_lights_image_based
    if (scene.extLightImageBased != -1)
    {
        if (extLightsImageBased == nullptr || static_cast<size_t>(scene.extLightImageBased) >= extLightsImageBased->size())
            throw std::invalid_argument("Scene references out-of-range image-based light index " + std::to_string(scene.extLightImageBased));
    }

    std::unordered_set<uint64_t> visited;
    for (auto nodeIndex : *scene.nodes)
    {
        if (nodeIndex >= nodes->size())
        {
            throw std::invalid_argument("Scene references out-of-range node index " + std::to_string(nodeIndex));
        }
        visited.insert(nodeIndex);
        updateRuntimeInfoWithNode(toReturn, (*nodes)[nodeIndex], visited);
    }

    return toReturn;
}

#define CHUNK_TYPE_JSON 0x4E4F534A
#define CHUNK_TYPE_BIN 0x004E4942

static uint32_t readLE32(const uint8_t *data, uint64_t offset)
{
    uint32_t value;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

static uint64_t readLE64(const uint8_t *data, uint64_t offset)
{
    uint64_t value;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

std::shared_ptr<GLTF> GLTF::loadGLB(uint8_t *data, uint64_t size)
{
    if (size < 12)
    {
        throw std::invalid_argument("Invalid glb size");
    }

    uint32_t magic = readLE32(data, 0);
    if (magic != 0x46546C67)
    {
        throw std::invalid_argument("Invalid GLB header");
    }

    uint32_t version = readLE32(data, 4);
    uint64_t totalLength;
    uint64_t headerSize;
    uint64_t chunkHeaderSize;
    uint64_t chunkAlignment;

    if (version == 2)
    {
        headerSize = 12;
        chunkHeaderSize = 8;  // length (4) + type (4)
        chunkAlignment = 4;
        totalLength = readLE32(data, 8);
    }
    else if (version == 3)
    {
        if (size < 16)
        {
            throw std::invalid_argument("Invalid glb size for version 3");
        }
        headerSize = 16;
        chunkHeaderSize = 16; // length (8) + type (4) + encoding (4)
        chunkAlignment = 8;
        totalLength = readLE64(data, 8);
    }
    else
    {
        throw std::invalid_argument("Unsupported GLB version: " + std::to_string(version));
    }

    if (totalLength != size)
    {
        throw std::invalid_argument("Wrong GLB size");
    }

    auto readChunk = [&](uint64_t &offset, uint64_t &chunkLength, uint32_t &chunkType, uint32_t &chunkEncoding) -> const uint8_t *
    {
        if (offset + chunkHeaderSize > size)
        {
            throw std::invalid_argument("GLB chunk header out of bounds");
        }

        if (version == 2)
        {
            chunkLength = readLE32(data, offset);
            chunkType = readLE32(data, offset + 4);
            chunkEncoding = 0;
        }
        else // version 3
        {
            chunkLength = readLE64(data, offset);
            chunkType = readLE32(data, offset + 8);
            chunkEncoding = readLE32(data, offset + 12);
        }

        if (offset + chunkHeaderSize + chunkLength > size)
        {
            throw std::invalid_argument("GLB chunk payload out of bounds");
        }

        const uint8_t *payload = data + offset + chunkHeaderSize;
        offset += chunkHeaderSize + chunkLength;

        // Chunks are aligned to the format's alignment boundary.
        if (chunkLength % chunkAlignment != 0)
        {
            offset += chunkAlignment - (chunkLength % chunkAlignment);
        }

        return payload;
    };

    // Chunk 0 must be JSON.
    uint64_t offset = headerSize;
    uint64_t chunk0Length;
    uint32_t chunk0Type;
    uint32_t chunk0Encoding;
    const uint8_t *chunk0Payload = readChunk(offset, chunk0Length, chunk0Type, chunk0Encoding);
    if (chunk0Type != CHUNK_TYPE_JSON)
    {
        throw std::invalid_argument("GLB chunk0 must be of type JSON");
    }

    if (chunk0Length > std::numeric_limits<size_t>::max())
    {
        throw std::invalid_argument("GLB JSON chunk too large");
    }
    auto chunk0 = std::string((char *)chunk0Payload, static_cast<size_t>(chunk0Length));
    auto toReturn = loadGLTF(chunk0);

    // Chunk 1 is optional and must be BIN if present.
    if (offset + chunkHeaderSize <= size)
    {
        uint64_t chunk1Length;
        uint32_t chunk1Type;
        uint32_t chunk1Encoding;
        const uint8_t *chunk1Payload = readChunk(offset, chunk1Length, chunk1Type, chunk1Encoding);

        if (chunk1Type == CHUNK_TYPE_BIN && toReturn->buffers->size() > 0)
        {
            auto &buffer0 = (*toReturn->buffers)[0];
            if (buffer0.byteLength != chunk1Length)
            {
                throw std::invalid_argument("GLB chunk1 incorrect length");
            }
            if (chunk1Length > std::numeric_limits<size_t>::max())
            {
                throw std::invalid_argument("GLB BIN chunk too large");
            }
            buffer0.data.resize(static_cast<size_t>(chunk1Length));
            if (chunk1Length > 0)
            {
                memcpy(buffer0.data.data(), chunk1Payload, static_cast<size_t>(chunk1Length));
            }

            // Now that the binary buffer is available, decompress any
            // KHR_draco_mesh_compression primitives that reference it, then
            // any EXT_meshopt_compression buffer views.
            GLTF::decompressDraco(toReturn);
            GLTF::decompressMeshopt(toReturn);
        }
    }

    return toReturn;
}

void GLTF::decompressDraco(std::shared_ptr<GLTF> &gltf)
{
    for (auto &mesh : *gltf->meshes)
    {
        for (auto &primitive : mesh.primitives)
        {
            if (!primitive.khrDracoMeshCompression) continue;
            auto &ext = *primitive.khrDracoMeshCompression;

            if (ext.bufferView < 0 ||
                ext.bufferView >= static_cast<int64_t>(gltf->bufferViews->size())) continue;

            const auto &bv = (*gltf->bufferViews)[ext.bufferView];
            if (bv.buffer < 0 ||
                bv.buffer >= static_cast<int64_t>(gltf->buffers->size())) continue;

            const auto &buf = (*gltf->buffers)[bv.buffer];
            if (buf.data.empty()) continue;

            draco::DecoderBuffer dracoBuffer;
            dracoBuffer.Init(
                reinterpret_cast<const char *>(buf.data.data() + bv.byteOffset),
                static_cast<size_t>(bv.byteLength));

            draco::Decoder decoder;
            auto statusor = decoder.DecodeMeshFromBuffer(&dracoBuffer);
            if (!statusor.ok()) continue;

            auto drcMesh = std::move(statusor).value();

            // Decode indices
            const int numFaces = drcMesh->num_faces();
            ext.decodedIndices.resize(static_cast<size_t>(numFaces) * 3);
            for (draco::FaceIndex fi(0); fi < numFaces; ++fi)
            {
                const auto &face = drcMesh->face(fi);
                ext.decodedIndices[fi.value() * 3 + 0] = face[0].value();
                ext.decodedIndices[fi.value() * 3 + 1] = face[1].value();
                ext.decodedIndices[fi.value() * 3 + 2] = face[2].value();
            }

            // Decode each attribute
            const int numPoints = drcMesh->num_points();
            for (auto &[semantic, dracoId] : ext.attributes)
            {
                const draco::PointAttribute *attr =
                    drcMesh->GetAttributeByUniqueId(static_cast<uint32_t>(dracoId));
                if (!attr) continue;

                const size_t byteStride = static_cast<size_t>(attr->byte_stride());
                std::vector<uint8_t> attrData(static_cast<size_t>(numPoints) * byteStride);

                for (draco::PointIndex pi(0); pi < numPoints; ++pi)
                {
                    const draco::AttributeValueIndex avi = attr->mapped_index(pi);
                    const uint8_t *src =
                        reinterpret_cast<const uint8_t *>(attr->GetAddress(avi));
                    memcpy(attrData.data() + pi.value() * byteStride, src, byteStride);
                }

                ext.decodedAttributes[semantic] = std::move(attrData);
            }
        }
    }
}

void GLTF::decompressMeshopt(std::shared_ptr<GLTF> &gltf)
{
    for (auto &bv : *gltf->bufferViews)
    {
        if (!bv.extMeshoptCompression) continue;
        auto &ext = *bv.extMeshoptCompression;

        if (ext.buffer < 0 ||
            ext.buffer >= static_cast<int64_t>(gltf->buffers->size())) continue;

        const auto &buf = (*gltf->buffers)[ext.buffer];
        if (buf.data.empty()) continue;
        if (ext.byteOffset + ext.byteLength > buf.data.size()) continue;

        const unsigned char *src =
            reinterpret_cast<const unsigned char *>(buf.data.data() + ext.byteOffset);
        size_t count  = static_cast<size_t>(ext.count);
        size_t stride = static_cast<size_t>(ext.byteStride);

        bv.decodedData.resize(count * stride);
        unsigned char *dst = bv.decodedData.data();

        int result = -1;
        if (ext.mode == EXTMeshoptMode::ATTRIBUTES)
        {
            result = meshopt_decodeVertexBuffer(dst, count, stride,
                                               src, static_cast<size_t>(ext.byteLength));
        }
        else if (ext.mode == EXTMeshoptMode::TRIANGLES)
        {
            // Two conventions exist for TRIANGLES mode:
            //   Spec:     byteStride = 3 * indexSize (6 or 12), count = triangle count
            //   gltfpack: byteStride = indexSize     (2 or 4),  count = index count
            size_t indexSize, indexCount;
            if (stride == 2 || stride == 4)
            {
                indexSize  = stride;   // gltfpack convention
                indexCount = count;
            }
            else
            {
                indexSize  = stride / 3;  // spec convention (byteStride must be 6 or 12)
                indexCount = count * 3;
            }
            result = meshopt_decodeIndexBuffer(dst, indexCount, indexSize,
                                              src, static_cast<size_t>(ext.byteLength));
        }
        else if (ext.mode == EXTMeshoptMode::INDICES)
        {
            result = meshopt_decodeIndexSequence(dst, count, stride,
                                                src, static_cast<size_t>(ext.byteLength));
        }

        if (result != 0)
        {
            bv.decodedData.clear();
            continue;
        }

        // Apply optional filter
        if (ext.filter == EXTMeshoptFilter::OCTAHEDRAL)
            meshopt_decodeFilterOct(dst, count, stride);
        else if (ext.filter == EXTMeshoptFilter::QUATERNION)
            meshopt_decodeFilterQuat(dst, count, stride);
        else if (ext.filter == EXTMeshoptFilter::EXPONENTIAL)
            meshopt_decodeFilterExp(dst, count, stride);
    }
}

/**
 * Parse a single mesh primitive, including all supported primitive extensions.
 */
static Primitive primitiveFromGLTF(const nlohmann::json &primitiveDef)
{
    if (!primitiveDef.contains("attributes") || !primitiveDef.at("attributes").is_object())
        throw std::invalid_argument("Primitive missing required 'attributes' object");
    auto& attributesDef = primitiveDef.at("attributes");
    std::unordered_map<std::string, uint64_t> attributes;
    for (auto &element : attributesDef.items())
    {
        attributes[element.key()] = element.value();
    }

    int64_t indices = primitiveDef.value("indices", -1);
    int64_t material = primitiveDef.value("material", -1);
    PrimitiveMode mode = (PrimitiveMode)primitiveDef.value("mode", 4);

    // Morph targets
    std::vector<std::unordered_map<std::string, uint64_t>> targets;
    if (primitiveDef.contains("targets") && primitiveDef.at("targets").is_array())
    {
        for (auto &targetDef : primitiveDef.at("targets"))
        {
            std::unordered_map<std::string, uint64_t> targetAttribs;
            if (targetDef.is_object())
            {
                for (auto &el : targetDef.items())
                {
                    targetAttribs[el.key()] = static_cast<uint64_t>(el.value());
                }
            }
            targets.push_back(std::move(targetAttribs));
        }
    }

    Primitive primitive(attributes, indices, material, mode, std::move(targets));

    nlohmann::json emptyPrimExt = nlohmann::json::object();
    nlohmann::json primExtDef = (primitiveDef.contains("extensions") && primitiveDef.at("extensions").is_object())
        ? primitiveDef["extensions"] : emptyPrimExt;

    // KHR_materials_variants
    auto variantMappingsDef = primExtDef["KHR_materials_variants"]["mappings"];
    if (variantMappingsDef.is_array())
    {
        for (uint32_t m = 0; m < variantMappingsDef.size(); m++)
        {
            auto md = variantMappingsDef[m];
            int64_t variantMaterial = md.value("material", int64_t(-1));
            std::vector<uint64_t> variantIndices;
            intListFromGLTF(variantIndices, md["variants"]);
            primitive.khrMaterialsVariantsMappings.emplace_back(variantMaterial, std::move(variantIndices));
        }
    }

    // KHR_draco_mesh_compression
    auto &dracoDef = primExtDef["KHR_draco_mesh_compression"];
    if (dracoDef.is_object())
    {
        int64_t dracoBufferView = dracoDef.value("bufferView", int64_t(-1));
        std::unordered_map<std::string, uint64_t> dracoAttribs;
        if (dracoDef["attributes"].is_object())
        {
            for (auto &el : dracoDef["attributes"].items())
                dracoAttribs[el.key()] = static_cast<uint64_t>(el.value());
        }
        primitive.khrDracoMeshCompression =
            std::make_shared<KHRDracoMeshCompression>(dracoBufferView, std::move(dracoAttribs));
    }

    // KHR_gaussian_splatting
    if (primExtDef["KHR_gaussian_splatting"].is_object())
    {
        auto& d = primExtDef["KHR_gaussian_splatting"];
        primitive.khrGaussianSplatting = std::make_shared<KHRGaussianSplatting>(
            d.value("kernel", std::string()),
            d.value("colorSpace", std::string()),
            d.value("projection", std::string("perspective")),
            d.value("sortingMethod", std::string("cameraDistance")));
    }

    // EXT_mesh_primitive_edge_visibility
    if (primExtDef["EXT_mesh_primitive_edge_visibility"].is_object())
    {
        auto& d = primExtDef["EXT_mesh_primitive_edge_visibility"];
        auto edgeVis = std::make_shared<EXTMeshPrimitiveEdgeVisibility>();
        edgeVis->visibility = d.value("visibility", int64_t(-1));
        edgeVis->material = d.value("material", int64_t(-1));
        edgeVis->silhouetteNormals = d.value("silhouetteNormals", int64_t(-1));
        auto lineStringsDef = d["lineStrings"];
        if (lineStringsDef.is_array())
        {
            for (uint32_t i = 0; i < lineStringsDef.size(); i++)
            {
                auto& ls = lineStringsDef[i];
                EXTMeshPrimitiveEdgeVisibilityLineString lineString;
                lineString.indices = ls.value("indices", int64_t(-1));
                lineString.material = ls.value("material", int64_t(-1));
                edgeVis->lineStrings.push_back(lineString);
            }
        }
        primitive.extMeshPrimitiveEdgeVisibility = edgeVis;
    }

    // EXT_mesh_primitive_restart
    if (primExtDef["EXT_mesh_primitive_restart"].is_object())
    {
        auto& d = primExtDef["EXT_mesh_primitive_restart"];
        auto restart = std::make_shared<EXTMeshPrimitiveRestart>();
        auto groupsDef = d["primitiveGroups"];
        if (groupsDef.is_array())
        {
            for (uint32_t i = 0; i < groupsDef.size(); i++)
            {
                auto& gd = groupsDef[i];
                EXTMeshPrimitiveRestartGroup group;
                intListFromGLTF(group.primitives, gd["primitives"]);
                group.indices = gd.value("indices", int64_t(-1));
                restart->primitiveGroups.push_back(std::move(group));
            }
        }
        primitive.extMeshPrimitiveRestart = restart;
    }

    return primitive;
}

std::shared_ptr<GLTF> GLTF::loadGLTF(const std::string &data)
{

    auto gltfDef = json::parse(data);

    // check version
    if (!gltfDef.contains("asset") || !gltfDef.at("asset").is_object() || !gltfDef.at("asset").contains("version"))
    {
        throw std::invalid_argument("Missing required 'asset.version' field");
    }
    std::string version = gltfDef["asset"]["version"];
    auto parts = split(version, ".");
    if (parts.size() < 2)
    {
        throw std::invalid_argument("Check version issue. Incorrect version format " + version);
    }
    int majorVersion = 0;
    int minorVersion = 0;
    try
    {
        majorVersion = std::stoi(parts[0]);
        minorVersion = std::stoi(parts[1]);
    }
    catch (...)
    {
        throw std::invalid_argument("Check version issue. Incorrect version format " + version);
    }
    if (majorVersion != 2 || minorVersion < 0)
    {
        throw std::invalid_argument("Check version issue. Expecting version 2.x. Found version " + version);
    }

    int64_t assetThumbnail = -1;
    auto assetDef = gltfDef["asset"];
    if (assetDef.is_object() && assetDef.contains("thumbnail") && assetDef.at("thumbnail").is_number())
    {
        assetThumbnail = assetDef.at("thumbnail").get<int64_t>();
    }

    // check implemented versions
    auto extensionsRequiredDef = gltfDef["extensionsRequired"];
    if (extensionsRequiredDef.is_array())
    {
        for (int a = 0; a < extensionsRequiredDef.size(); a++)
        {
            std::string extension = extensionsRequiredDef[a];
            if (std::find(implementedExtensions.begin(), implementedExtensions.end(), extension) == implementedExtensions.end())
            {
                throw std::invalid_argument("Required extension '" + extension + "' not implemented.");
            }
        }
    }

    // Buffers
    auto buffers = std::make_shared<std::vector<Buffer>>();
    auto buffersDef = gltfDef["buffers"];
    if (buffersDef.is_array())
    {

        if (buffersDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many buffers: " + std::to_string(buffersDef.size()));
        }
        buffers->reserve(buffersDef.size());
        for (uint32_t a = 0; a < buffersDef.size(); a++)
        {
            auto buffer = buffersDef[a];

            auto name = buffer.value("name", "");

            if (!buffer.contains("byteLength") || !buffer.at("byteLength").is_number())
                throw std::invalid_argument("Buffer[" + std::to_string(a) + "] missing required 'byteLength' field");
            uint64_t byteLength = buffer.at("byteLength").get<uint64_t>();
            if (byteLength > GLTF_MAX_BUFFER_BYTE_LENGTH)
            {
                throw std::invalid_argument("Buffer[" + std::to_string(a) + "] byteLength exceeds maximum allowed size");
            }

            std::vector<uint8_t> data = {};

            std::string uri = buffer.value("uri", "");
            if (startsWith(uri, "data:"))
            {
                auto parts = split(uri, ";base64,");
                if (parts.size() > 1)
                {
                    auto &lastPart = parts[parts.size() - 1];
                    auto decoded = base64_decode(lastPart);
                    data = std::vector<uint8_t>(decoded.begin(), decoded.end());
                    uri = "";
                    if (data.size() != byteLength)
                    {
                        throw std::invalid_argument("Buffer[" + std::to_string(a) + "] uri payload length error.");
                    }
                }
            }

            buffers->emplace_back(name, uri, byteLength, data);
            buffers->back().uid = parseUid(buffer);
        }
    }

    // BufferViews
    auto bufferViews = std::make_shared<std::vector<BufferView>>();
    auto bufferViewsDef = gltfDef["bufferViews"];
    if (bufferViewsDef.is_array())
    {

        if (bufferViewsDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many bufferViews: " + std::to_string(bufferViewsDef.size()));
        }
        bufferViews->reserve(bufferViewsDef.size());
        for (uint32_t a = 0; a < bufferViewsDef.size(); a++)
        {
            auto bufferView = bufferViewsDef[a];

            auto name = bufferView.value("name", "");
            if (!bufferView.contains("buffer") || !bufferView.at("buffer").is_number())
                throw std::invalid_argument("BufferView[" + std::to_string(a) + "] missing required 'buffer' field");
            if (!bufferView.contains("byteLength") || !bufferView.at("byteLength").is_number())
                throw std::invalid_argument("BufferView[" + std::to_string(a) + "] missing required 'byteLength' field");
            uint64_t buffer = bufferView.at("buffer").get<uint64_t>();
            uint64_t byteOffset = bufferView.value("byteOffset", std::uint64_t(0));
            uint64_t byteLength = bufferView.at("byteLength").get<uint64_t>();
            int64_t byteStride = bufferView.value("byteStride", -1);
            BufferViewTarget target = (BufferViewTarget)bufferView.value("target", 0);
            bufferViews->emplace_back(name, buffer, byteOffset, byteLength, byteStride, target);
            bufferViews->back().uid = parseUid(bufferView);

            // EXT_meshopt_compression / KHR_meshopt_compression (same structure, two names)
            auto &bvExts = bufferViewsDef[a]["extensions"];
            json meshoptDef;
            if (bvExts.is_object())
            {
                if (bvExts.contains("EXT_meshopt_compression"))
                    meshoptDef = bvExts["EXT_meshopt_compression"];
                else if (bvExts.contains("KHR_meshopt_compression"))
                    meshoptDef = bvExts["KHR_meshopt_compression"];
            }
            if (meshoptDef.is_object())
            {
                int64_t  meshoptBuffer     = meshoptDef.value("buffer",     int64_t(-1));
                uint64_t meshoptByteOffset = meshoptDef.value("byteOffset", uint64_t(0));
                if (!meshoptDef.contains("byteLength") || !meshoptDef.at("byteLength").is_number())
                    throw std::invalid_argument("EXT_meshopt_compression missing required 'byteLength' field");
                if (!meshoptDef.contains("byteStride") || !meshoptDef.at("byteStride").is_number())
                    throw std::invalid_argument("EXT_meshopt_compression missing required 'byteStride' field");
                if (!meshoptDef.contains("count") || !meshoptDef.at("count").is_number())
                    throw std::invalid_argument("EXT_meshopt_compression missing required 'count' field");
                uint64_t meshoptByteLength = meshoptDef.at("byteLength").get<uint64_t>();
                uint64_t meshoptByteStride = meshoptDef.at("byteStride").get<uint64_t>();
                uint64_t meshoptCount      = meshoptDef.at("count").get<uint64_t>();

                std::string modeStr   = meshoptDef.value("mode",   "ATTRIBUTES");
                std::string filterStr = meshoptDef.value("filter", "NONE");

                EXTMeshoptMode mode = EXTMeshoptMode::ATTRIBUTES;
                if (modeStr == "TRIANGLES") mode = EXTMeshoptMode::TRIANGLES;
                else if (modeStr == "INDICES") mode = EXTMeshoptMode::INDICES;

                EXTMeshoptFilter filter = EXTMeshoptFilter::NONE;
                if (filterStr == "OCTAHEDRAL")   filter = EXTMeshoptFilter::OCTAHEDRAL;
                else if (filterStr == "QUATERNION")  filter = EXTMeshoptFilter::QUATERNION;
                else if (filterStr == "EXPONENTIAL") filter = EXTMeshoptFilter::EXPONENTIAL;

                bufferViews->back().extMeshoptCompression =
                    std::make_shared<EXTMeshoptCompression>(
                        meshoptBuffer, meshoptByteOffset, meshoptByteLength,
                        meshoptByteStride, meshoptCount, mode, filter);
            }
        }
    }

    // Accessors
    auto accessors = std::make_shared<std::vector<Accessor>>();
    auto accessorsDef = gltfDef["accessors"];
    if (accessorsDef.is_array())
    {

        if (accessorsDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many accessors: " + std::to_string(accessorsDef.size()));
        }
        accessors->reserve(accessorsDef.size());
        for (uint32_t a = 0; a < accessorsDef.size(); a++)
        {
            auto accessor = accessorsDef[a];

            auto name = accessor.value("name", "");
            int64_t bufferView = accessor.value("bufferView", -1);
            uint64_t byteOffset = accessor.value("byteOffset", std::uint64_t(0));
            if (!accessor.contains("componentType") || !accessor.at("componentType").is_number())
                throw std::invalid_argument("Accessor[" + std::to_string(a) + "] missing required 'componentType' field");
            if (!accessor.contains("count") || !accessor.at("count").is_number())
                throw std::invalid_argument("Accessor[" + std::to_string(a) + "] missing required 'count' field");
            if (!accessor.contains("type") || !accessor.at("type").is_string())
                throw std::invalid_argument("Accessor[" + std::to_string(a) + "] missing required 'type' field");
            ComponentType componentType = (ComponentType)accessor.at("componentType").get<uint64_t>();
            bool normalized = accessor.value("normalized", false);
            uint64_t count = accessor.at("count").get<uint64_t>();
            std::string type = accessor.at("type").get<std::string>();
            AccessorType t;
            if (type == "SCALAR")
            {
                t = AccessorType::acScalar;
            }
            else if (type == "VEC2")
            {
                t = AccessorType::acVec2;
            }
            else if (type == "VEC3")
            {
                t = AccessorType::acVec3;
            }
            else if (type == "VEC4")
            {
                t = AccessorType::acVec4;
            }
            else if (type == "MAT2")
            {
                t = AccessorType::acMat2;
            }
            else if (type == "MAT3")
            {
                t = AccessorType::acMat3;
            }
            else if (type == "MAT4")
            {
                t = AccessorType::acMat4;
            }
            else
            {
                throw std::invalid_argument("Unsupported accessor type");
            }

            accessors->emplace_back(name, bufferView, byteOffset, componentType, normalized, count, t);
            accessors->back().uid = parseUid(accessor);

            // sparse
            if (accessor.contains("sparse") && accessor.at("sparse").is_object())
            {
                auto &spDef = accessor.at("sparse");
                uint64_t spCount = spDef.value("count", uint64_t(0));

                auto &idxDef = spDef.at("indices");
                Accessor::SparseIndices spIdx{
                    idxDef.value("bufferView", uint64_t(0)),
                    idxDef.value("byteOffset", uint64_t(0)),
                    (ComponentType)(uint64_t)idxDef.at("componentType")
                };

                auto &valDef = spDef.at("values");
                Accessor::SparseValues spVal{
                    valDef.value("bufferView", uint64_t(0)),
                    valDef.value("byteOffset", uint64_t(0))
                };

                if (spCount > count)
                    throw std::invalid_argument("Accessor sparse count exceeds accessor count");

                accessors->back().sparse = std::make_shared<Accessor::Sparse>(
                    Accessor::Sparse{spCount, spIdx, spVal});
            }

            // min / max
            if (accessor.contains("min") && accessor.at("min").is_array())
            {
                std::vector<GLTF_REAL_NUMBER_TYPE> minVec;
                for (auto &v : accessor.at("min"))
                    minVec.push_back(v.get<GLTF_REAL_NUMBER_TYPE>());
                accessors->back().min = std::move(minVec);
            }
            if (accessor.contains("max") && accessor.at("max").is_array())
            {
                std::vector<GLTF_REAL_NUMBER_TYPE> maxVec;
                for (auto &v : accessor.at("max"))
                    maxVec.push_back(v.get<GLTF_REAL_NUMBER_TYPE>());
                accessors->back().max = std::move(maxVec);
            }
        }
    }

    // Cameras
    auto cameras = std::make_shared<std::vector<std::shared_ptr<Camera>>>();
    auto camerasDef = gltfDef["cameras"];
    if (camerasDef.is_array())
    {
        cameras->reserve(camerasDef.size());
        for (uint32_t a = 0; a < camerasDef.size(); a++)
        {
            auto camera = camerasDef[a];
            std::string name = camera.value("name", "");
            if (!camera.contains("type") || !camera.at("type").is_string())
                throw std::invalid_argument("Camera[" + std::to_string(a) + "] missing required 'type' field");
            std::string type = camera.at("type").get<std::string>();
            if (type == "perspective")
            {
                if (!camera.contains("perspective") || !camera.at("perspective").is_object())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] missing required 'perspective' object");
                auto& persp = camera.at("perspective");
                if (!persp.contains("yfov") || !persp.at("yfov").is_number())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] perspective missing required 'yfov' field");
                if (!persp.contains("znear") || !persp.at("znear").is_number())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] perspective missing required 'znear' field");
                std::optional<GLTF_REAL_NUMBER_TYPE> aspectRatio;
                if (persp.contains("aspectRatio") && persp.at("aspectRatio").is_number())
                    aspectRatio = persp.at("aspectRatio").get<GLTF_REAL_NUMBER_TYPE>();
                GLTF_REAL_NUMBER_TYPE yfov = persp.at("yfov").get<GLTF_REAL_NUMBER_TYPE>();
                std::optional<GLTF_REAL_NUMBER_TYPE> zfar;
                if (persp.contains("zfar") && persp.at("zfar").is_number())
                    zfar = persp.at("zfar").get<GLTF_REAL_NUMBER_TYPE>();
                GLTF_REAL_NUMBER_TYPE znear = persp.at("znear").get<GLTF_REAL_NUMBER_TYPE>();
                cameras->push_back(std::make_shared<PerspectiveCamera>(name, aspectRatio, yfov, zfar, znear));
                cameras->back()->uid = parseUid(camera);
            }
            else if (type == "orthographic")
            {
                if (!camera.contains("orthographic") || !camera.at("orthographic").is_object())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] missing required 'orthographic' object");
                auto& ortho = camera.at("orthographic");
                if (!ortho.contains("xmag") || !ortho.at("xmag").is_number())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] orthographic missing required 'xmag' field");
                if (!ortho.contains("ymag") || !ortho.at("ymag").is_number())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] orthographic missing required 'ymag' field");
                if (!ortho.contains("zfar") || !ortho.at("zfar").is_number())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] orthographic missing required 'zfar' field");
                if (!ortho.contains("znear") || !ortho.at("znear").is_number())
                    throw std::invalid_argument("Camera[" + std::to_string(a) + "] orthographic missing required 'znear' field");
                GLTF_REAL_NUMBER_TYPE xmag = ortho.at("xmag").get<GLTF_REAL_NUMBER_TYPE>();
                GLTF_REAL_NUMBER_TYPE ymag = ortho.at("ymag").get<GLTF_REAL_NUMBER_TYPE>();
                GLTF_REAL_NUMBER_TYPE zfar = ortho.at("zfar").get<GLTF_REAL_NUMBER_TYPE>();
                GLTF_REAL_NUMBER_TYPE znear = ortho.at("znear").get<GLTF_REAL_NUMBER_TYPE>();
                cameras->push_back(std::make_shared<OrthographicCamera>(name, xmag, ymag, zfar, znear));
                cameras->back()->uid = parseUid(camera);
            }
            else
            {
                throw std::invalid_argument("Unsupported camera type");
            }
        }
    }

    // Images
    auto images = std::make_shared<std::vector<Image>>();
    auto imagesDef = gltfDef["images"];
    if (imagesDef.is_array())
    {
        images->reserve(imagesDef.size());
        for (uint32_t a = 0; a < imagesDef.size(); a++)
        {
            auto image = imagesDef[a];

            std::string name = image.value("name", "");

            std::string mimeType = image.value("mime", "");

            int64_t bufferView = image.value("bufferView", -1);

            std::vector<uint8_t> data = {};

            std::string uri = image.value("uri", "");
            if (startsWith(uri, "data:"))
            {
                auto parts = split(uri, ";base64,");
                if (parts.size() > 1)
                {
                    auto &lastPart = parts[parts.size() - 1];
                    auto decoded = base64_decode(lastPart);
                    data = std::vector<uint8_t>(decoded.begin(), decoded.end());
                    uri = "";

                    auto mimeparts = split(parts[0], ":");
                    if (mimeparts.size() > 1)
                    {
                        mimeType = mimeparts[1];
                    }
                }
            }

            images->emplace_back(name, uri, mimeType, bufferView, data);
            images->back().uid = parseUid(image);
        }
    }

    // Files (glTF 2.1 unified external file references)
    auto files = std::make_shared<std::vector<File>>();
    auto filesDef = gltfDef["files"];
    if (filesDef.is_array())
    {
        if (filesDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many files: " + std::to_string(filesDef.size()));
        }
        files->reserve(filesDef.size());
        for (uint32_t a = 0; a < filesDef.size(); a++)
        {
            auto file = filesDef[a];
            auto name = file.value("name", "");

            if (!file.contains("mimeType") || !file.at("mimeType").is_string())
                throw std::invalid_argument("File[" + std::to_string(a) + "] missing required 'mimeType' field");
            std::string mimeType = file.at("mimeType").get<std::string>();

            std::string uri = file.value("uri", "");
            int64_t bufferView = file.value("bufferView", -1);

            std::vector<uint8_t> data = {};
            if (startsWith(uri, "data:"))
            {
                auto parts = split(uri, ";base64,");
                if (parts.size() > 1)
                {
                    auto &lastPart = parts[parts.size() - 1];
                    auto decoded = base64_decode(lastPart);
                    data = std::vector<uint8_t>(decoded.begin(), decoded.end());
                    uri = "";
                }
            }

            files->emplace_back(name, uri, bufferView, mimeType, data);
            files->back().uid = parseUid(file);
        }
    }

    // Textures
    auto textures = std::make_shared<std::vector<Texture>>();
    auto texturesDef = gltfDef["textures"];
    if (texturesDef.is_array())
    {
        textures->reserve(texturesDef.size());
        for (uint32_t a = 0; a < texturesDef.size(); a++)
        {
            auto texture = texturesDef[a];

            auto name = texture.value("name","");
            int64_t sampler = texture.value("sampler", -1);
            int64_t source = texture.value("source", -1);

            // EXT_texture_webp
            int64_t extTextureWebp = -1;
            if (texture.contains("extensions") && texture["extensions"]["EXT_texture_webp"].is_object()) {
                extTextureWebp = texture["extensions"]["EXT_texture_webp"].value("source", -1);
            }

            // KHR_texture_basisu
            int64_t khrTextureBasisuSource = -1;
            if (texture.contains("extensions") && texture["extensions"]["KHR_texture_basisu"].is_object()) {
                khrTextureBasisuSource = texture["extensions"]["KHR_texture_basisu"].value("source", -1);
            }

            // EXT_texture_astc
            int64_t extTextureAstc = -1;
            if (texture.contains("extensions") && texture["extensions"]["EXT_texture_astc"].is_object()) {
                extTextureAstc = texture["extensions"]["EXT_texture_astc"].value("source", -1);
            }

            textures->emplace_back(name, sampler, source, extTextureWebp, khrTextureBasisuSource, extTextureAstc);
            textures->back().uid = parseUid(texture);
        }
    }

    // Samplers
    auto samplers = std::make_shared<std::vector<Sampler>>();
    auto samplersDef = gltfDef["samplers"];
    if (samplersDef.is_array())
    {
        samplers->reserve(samplersDef.size());
        for (uint32_t a = 0; a < samplersDef.size(); a++)
        {
            auto sampler = samplersDef[a];

            auto name = sampler.value("name","");

            FilterMode magFilter = filterModeFromGLTF(sampler["magFilter"]);
            FilterMode minFilter = filterModeFromGLTF(sampler["minFilter"]);
            WrapMode wrapS = (WrapMode)sampler.value("wrapS", uint64_t(10497));
            WrapMode wrapT = (WrapMode)sampler.value("wrapT", uint64_t(10497));

            samplers->emplace_back(name, magFilter, minFilter, wrapS, wrapT);
            samplers->back().uid = parseUid(sampler);
        }
    }

    // Materials
    auto materials = std::make_shared<std::vector<Material>>();
    auto materialsDef = gltfDef["materials"];
    if (materialsDef.is_array())
    {
        if (materialsDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many materials: " + std::to_string(materialsDef.size()));
        }
        materials->reserve(materialsDef.size());
        for (uint32_t a = 0; a < materialsDef.size(); a++)
        {
            auto materialDef = materialsDef[a];

            auto name = materialDef.value("name","");

            auto pbrMetallicRoughnessDef = materialDef["pbrMetallicRoughness"];
            std::shared_ptr<PBRMetallicRoughness> pbrMetallicRoughness = nullptr;
            if (pbrMetallicRoughnessDef.is_object())
            {

                auto baseColorFactor = vec4FromGLTF(pbrMetallicRoughnessDef["baseColorFactor"], systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE>(1, 1, 1, 1));

                auto baseColorTexture = textureInfoFromGLTF(pbrMetallicRoughnessDef["baseColorTexture"]);

                GLTF_REAL_NUMBER_TYPE metallicFactor = pbrMetallicRoughnessDef.value("metallicFactor", 1);

                GLTF_REAL_NUMBER_TYPE roughnessFactor = pbrMetallicRoughnessDef.value("roughnessFactor", 1);

                auto metallicRoughnessTexture = textureInfoFromGLTF(pbrMetallicRoughnessDef["metallicRoughnessTexture"]);

                pbrMetallicRoughness = std::make_shared<PBRMetallicRoughness>(
                    baseColorFactor,
                    baseColorTexture,
                    metallicFactor,
                    roughnessFactor,
                    metallicRoughnessTexture);
            }

            auto normalTextureDef = materialDef["normalTexture"];
            std::shared_ptr<NormalTextureInfo> normalTexture = nullptr;
            if (normalTextureDef.is_object())
            {
                if (!normalTextureDef.contains("index") || !normalTextureDef.at("index").is_number())
                    throw std::invalid_argument("NormalTextureInfo missing required 'index' field");
                normalTexture = std::make_shared<NormalTextureInfo>(
                    normalTextureDef["index"],
                    normalTextureDef.value("texCoord", 0),
                    normalTextureDef.value("scale", 1));
                if (normalTextureDef.contains("extensions"))
                    normalTexture->khrTextureTransform = khrTextureTransformFromGLTF(normalTextureDef["extensions"]["KHR_texture_transform"]);
            }

            auto occlusionTextureDef = materialDef["occlusionTexture"];
            std::shared_ptr<OcclusionTextureInfo> occlusionTexture = nullptr;
            if (occlusionTextureDef.is_object())
            {
                if (!occlusionTextureDef.contains("index") || !occlusionTextureDef.at("index").is_number())
                    throw std::invalid_argument("OcclusionTextureInfo missing required 'index' field");
                occlusionTexture = std::make_shared<OcclusionTextureInfo>(
                    occlusionTextureDef["index"],
                    occlusionTextureDef.value("texCoord", 0),
                    occlusionTextureDef.value("strength", 1));
                if (occlusionTextureDef.contains("extensions"))
                    occlusionTexture->khrTextureTransform = khrTextureTransformFromGLTF(occlusionTextureDef["extensions"]["KHR_texture_transform"]);
            }

            auto emissiveTexture = textureInfoFromGLTF(materialDef["emissiveTexture"]);

            auto emissiveFactor = vec3FromGLTF(materialDef["emissiveFactor"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(0, 0, 0));

            auto alphaMode = alphaModeFromGLTF(materialDef["alphaMode"]);

            GLTF_REAL_NUMBER_TYPE alphaCutoff = materialDef.value("alphaCutoff", 0.5);

            bool doubleSided = materialDef.value("doubleSided", false);

            nlohmann::json emptyExtensions = nlohmann::json::object();
            nlohmann::json& matExtDef = (materialDef.contains("extensions") && materialDef.at("extensions").is_object())
                ? materialDef["extensions"] : emptyExtensions;

            // KHR_materials_unlit
            bool khrMaterialsUnlit = matExtDef["KHR_materials_unlit"].is_object();

            // KHR_materials_emissive_strength
            GLTF_REAL_NUMBER_TYPE khrMaterialsEmissiveStrength = 1.0;
            if (matExtDef["KHR_materials_emissive_strength"].is_object()) {
                khrMaterialsEmissiveStrength = matExtDef["KHR_materials_emissive_strength"].value("emissiveStrength", 1.0);
            }

            // KHR_materials_ior
            GLTF_REAL_NUMBER_TYPE khrMaterialsIor = 1.5;
            if (matExtDef["KHR_materials_ior"].is_object()) {
                khrMaterialsIor = matExtDef["KHR_materials_ior"].value("ior", 1.5);
            }

            // KHR_materials_dispersion
            GLTF_REAL_NUMBER_TYPE khrMaterialsDispersion = 0.0;
            if (matExtDef["KHR_materials_dispersion"].is_object()) {
                khrMaterialsDispersion = matExtDef["KHR_materials_dispersion"].value("dispersion", 0.0);
            }

            auto& mat = materials->emplace_back(
                name,
                pbrMetallicRoughness,
                normalTexture,
                occlusionTexture,
                emissiveTexture,
                emissiveFactor,
                alphaMode,
                alphaCutoff,
                doubleSided,
                khrMaterialsUnlit,
                khrMaterialsEmissiveStrength,
                khrMaterialsIor,
                khrMaterialsDispersion);
            mat.uid = parseUid(materialDef);

            // KHR_xmp_json_ld
            if (matExtDef["KHR_xmp_json_ld"].is_object())
            {
                mat.khrXmpPacket = matExtDef["KHR_xmp_json_ld"].value("packet", int64_t(-1));
            }

            // KHR_materials_transmission
            if (matExtDef["KHR_materials_transmission"].is_object())
            {
                auto& d = matExtDef["KHR_materials_transmission"];
                mat.khrMaterialsTransmission = std::make_shared<KHRMaterialsTransmission>(
                    d.value("transmissionFactor", 0.0),
                    textureInfoFromGLTF(d["transmissionTexture"]));
            }

            // KHR_materials_clearcoat
            if (matExtDef["KHR_materials_clearcoat"].is_object())
            {
                auto& d = matExtDef["KHR_materials_clearcoat"];
                std::shared_ptr<NormalTextureInfo> ccNormal = nullptr;
                if (d["clearcoatNormalTexture"].is_object())
                {
                    auto& nd = d["clearcoatNormalTexture"];
                    if (!nd.contains("index") || !nd.at("index").is_number())
                        throw std::invalid_argument("clearcoatNormalTexture missing required 'index' field");
                    ccNormal = std::make_shared<NormalTextureInfo>(
                        nd["index"], nd.value("texCoord", 0), nd.value("scale", 1.0));
                    if (nd.contains("extensions") && nd.at("extensions").is_object() &&
                        nd.at("extensions").contains("KHR_texture_transform"))
                        ccNormal->khrTextureTransform = khrTextureTransformFromGLTF(nd["extensions"]["KHR_texture_transform"]);
                }
                mat.khrMaterialsClearcoat = std::make_shared<KHRMaterialsClearcoat>(
                    d.value("clearcoatFactor", 0.0),
                    textureInfoFromGLTF(d["clearcoatTexture"]),
                    d.value("clearcoatRoughnessFactor", 0.0),
                    textureInfoFromGLTF(d["clearcoatRoughnessTexture"]),
                    ccNormal);
            }

            // KHR_materials_sheen
            if (matExtDef["KHR_materials_sheen"].is_object())
            {
                auto& d = matExtDef["KHR_materials_sheen"];
                mat.khrMaterialsSheen = std::make_shared<KHRMaterialsSheen>(
                    vec3FromGLTF(d["sheenColorFactor"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(0, 0, 0)),
                    textureInfoFromGLTF(d["sheenColorTexture"]),
                    d.value("sheenRoughnessFactor", 0.0),
                    textureInfoFromGLTF(d["sheenRoughnessTexture"]));
            }

            // KHR_materials_specular
            if (matExtDef["KHR_materials_specular"].is_object())
            {
                auto& d = matExtDef["KHR_materials_specular"];
                mat.khrMaterialsSpecular = std::make_shared<KHRMaterialsSpecular>(
                    d.value("specularFactor", 1.0),
                    textureInfoFromGLTF(d["specularTexture"]),
                    vec3FromGLTF(d["specularColorFactor"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(1, 1, 1)),
                    textureInfoFromGLTF(d["specularColorTexture"]));
            }

            // KHR_materials_volume
            if (matExtDef["KHR_materials_volume"].is_object())
            {
                auto& d = matExtDef["KHR_materials_volume"];
                mat.khrMaterialsVolume = std::make_shared<KHRMaterialsVolume>(
                    d.value("thicknessFactor", 0.0),
                    textureInfoFromGLTF(d["thicknessTexture"]),
                    d.value("attenuationDistance", std::numeric_limits<GLTF_REAL_NUMBER_TYPE>::infinity()),
                    vec3FromGLTF(d["attenuationColor"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(1, 1, 1)));
            }

            // KHR_materials_iridescence
            if (matExtDef["KHR_materials_iridescence"].is_object())
            {
                auto& d = matExtDef["KHR_materials_iridescence"];
                mat.khrMaterialsIridescence = std::make_shared<KHRMaterialsIridescence>(
                    d.value("iridescenceFactor", 0.0),
                    textureInfoFromGLTF(d["iridescenceTexture"]),
                    d.value("iridescenceIor", 1.3),
                    d.value("iridescenceThicknessMinimum", 100.0),
                    d.value("iridescenceThicknessMaximum", 400.0),
                    textureInfoFromGLTF(d["iridescenceThicknessTexture"]));
            }

            // KHR_materials_anisotropy
            if (matExtDef["KHR_materials_anisotropy"].is_object())
            {
                auto& d = matExtDef["KHR_materials_anisotropy"];
                mat.khrMaterialsAnisotropy = std::make_shared<KHRMaterialsAnisotropy>(
                    d.value("anisotropyStrength", 0.0),
                    d.value("anisotropyRotation", 0.0),
                    textureInfoFromGLTF(d["anisotropyTexture"]));
            }

            // KHR_materials_diffuse_transmission
            if (matExtDef["KHR_materials_diffuse_transmission"].is_object())
            {
                auto& d = matExtDef["KHR_materials_diffuse_transmission"];
                mat.khrMaterialsDiffuseTransmission = std::make_shared<KHRMaterialsDiffuseTransmission>(
                    d.value("diffuseTransmissionFactor", 0.0),
                    textureInfoFromGLTF(d["diffuseTransmissionTexture"]),
                    vec3FromGLTF(d["diffuseTransmissionColorFactor"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(1, 1, 1)),
                    textureInfoFromGLTF(d["diffuseTransmissionColorTexture"]));
            }
        }
    }

    // Meshes
    auto meshes = std::make_shared<std::vector<Mesh>>();
    auto meshesDef = gltfDef["meshes"];
    if (meshesDef.is_array())
    {
        if (meshesDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many meshes: " + std::to_string(meshesDef.size()));
        }
        meshes->reserve(meshesDef.size());
        for (uint32_t a = 0; a < meshesDef.size(); a++)
        {
            auto meshDef = meshesDef[a];

            auto name = meshDef.value("name","");

            if (!meshDef.contains("primitives") || !meshDef.at("primitives").is_array())
                throw std::invalid_argument("Mesh[" + std::to_string(a) + "] missing required 'primitives' array");
            auto primitivesDef = meshDef.at("primitives");
            std::vector<Primitive> primitives;
            for (int a = 0; a < (int)primitivesDef.size(); a++)
            {
                primitives.push_back(primitiveFromGLTF(primitivesDef[a]));
            }

            std::vector<GLTF_REAL_NUMBER_TYPE> weights;
            realListFromGLTF(weights, meshDef["weights"]);

            meshes->emplace_back(name, primitives, weights);
            meshes->back().uid = parseUid(meshDef);

            nlohmann::json emptyMeshExt = nlohmann::json::object();
            nlohmann::json& meshExtDef = (meshDef.contains("extensions") && meshDef.at("extensions").is_object())
                ? meshDef["extensions"] : emptyMeshExt;
            if (meshExtDef["KHR_xmp_json_ld"].is_object())
            {
                meshes->back().khrXmpPacket = meshExtDef["KHR_xmp_json_ld"].value("packet", int64_t(-1));
            }

            // EXT_mesh_manifold
            if (meshExtDef["EXT_mesh_manifold"].is_object())
            {
                auto& d = meshExtDef["EXT_mesh_manifold"];
                Primitive manifoldPrimitive = d.contains("manifoldPrimitive")
                    ? primitiveFromGLTF(d["manifoldPrimitive"])
                    : Primitive({}, -1, -1, PrimitiveMode::pmTriangles, {});
                int64_t mergeIndices = d.value("mergeIndices", int64_t(-1));
                int64_t mergeValues = d.value("mergeValues", int64_t(-1));
                meshes->back().extMeshManifold =
                    std::make_shared<EXTMeshManifold>(manifoldPrimitive, mergeIndices, mergeValues);
            }
        }
    }

    // Shapes (glTF 2.1 implicit geometric primitives)
    auto shapes = std::make_shared<std::vector<std::shared_ptr<Shape>>>();
    auto shapesDef = gltfDef["shapes"];
    if (shapesDef.is_array())
    {
        if (shapesDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many shapes: " + std::to_string(shapesDef.size()));
        }
        shapes->reserve(shapesDef.size());
        for (uint32_t a = 0; a < shapesDef.size(); a++)
        {
            auto shape = shapesDef[a];
            auto name = shape.value("name", "");

            if (!shape.contains("type") || !shape.at("type").is_string())
                throw std::invalid_argument("Shape[" + std::to_string(a) + "] missing required 'type' field");
            std::string type = shape.at("type").get<std::string>();

            std::shared_ptr<Shape> parsedShape = nullptr;
            if (type == "box")
            {
                auto &boxDef = shape["box"];
                if (!boxDef.is_object())
                    throw std::invalid_argument("Shape[" + std::to_string(a) + "] box missing 'box' object");
                auto size = vec3FromGLTF(boxDef["size"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(1.0, 1.0, 1.0));
                parsedShape = std::make_shared<BoxShape>(name, size);
            }
            else if (type == "sphere")
            {
                auto &sphereDef = shape["sphere"];
                if (!sphereDef.is_object())
                    throw std::invalid_argument("Shape[" + std::to_string(a) + "] sphere missing 'sphere' object");
                GLTF_REAL_NUMBER_TYPE radius = sphereDef.value("radius", GLTF_REAL_NUMBER_TYPE(1.0));
                parsedShape = std::make_shared<SphereShape>(name, radius);
            }
            else if (type == "capsule")
            {
                auto &capsuleDef = shape["capsule"];
                if (!capsuleDef.is_object())
                    throw std::invalid_argument("Shape[" + std::to_string(a) + "] capsule missing 'capsule' object");
                GLTF_REAL_NUMBER_TYPE height = capsuleDef.value("height", GLTF_REAL_NUMBER_TYPE(1.0));
                GLTF_REAL_NUMBER_TYPE radiusTop = capsuleDef.value("radiusTop", GLTF_REAL_NUMBER_TYPE(0.5));
                GLTF_REAL_NUMBER_TYPE radiusBottom = capsuleDef.value("radiusBottom", GLTF_REAL_NUMBER_TYPE(0.5));
                parsedShape = std::make_shared<CapsuleShape>(name, height, radiusTop, radiusBottom);
            }
            else if (type == "cylinder")
            {
                auto &cylinderDef = shape["cylinder"];
                if (!cylinderDef.is_object())
                    throw std::invalid_argument("Shape[" + std::to_string(a) + "] cylinder missing 'cylinder' object");
                GLTF_REAL_NUMBER_TYPE height = cylinderDef.value("height", GLTF_REAL_NUMBER_TYPE(1.0));
                GLTF_REAL_NUMBER_TYPE radiusTop = cylinderDef.value("radiusTop", GLTF_REAL_NUMBER_TYPE(0.5));
                GLTF_REAL_NUMBER_TYPE radiusBottom = cylinderDef.value("radiusBottom", GLTF_REAL_NUMBER_TYPE(0.5));
                parsedShape = std::make_shared<CylinderShape>(name, height, radiusTop, radiusBottom);
            }
            else if (type == "plane")
            {
                auto &planeDef = shape["plane"];
                if (!planeDef.is_object())
                    throw std::invalid_argument("Shape[" + std::to_string(a) + "] plane missing 'plane' object");
                bool doubleSided = planeDef.value("doubleSided", false);
                std::optional<GLTF_REAL_NUMBER_TYPE> sizeX;
                std::optional<GLTF_REAL_NUMBER_TYPE> sizeZ;
                if (planeDef.contains("sizeX") && planeDef.at("sizeX").is_number())
                    sizeX = planeDef.at("sizeX").get<GLTF_REAL_NUMBER_TYPE>();
                if (planeDef.contains("sizeZ") && planeDef.at("sizeZ").is_number())
                    sizeZ = planeDef.at("sizeZ").get<GLTF_REAL_NUMBER_TYPE>();
                parsedShape = std::make_shared<PlaneShape>(name, doubleSided, sizeX, sizeZ);
            }
            else
            {
                throw std::invalid_argument("Shape[" + std::to_string(a) + "] unsupported type: " + type);
            }

            if (parsedShape)
            {
                parsedShape->uid = parseUid(shape);
                shapes->push_back(parsedShape);
            }
        }
    }

    // External assets (glTF 2.1 complex scene composition)
    auto externalAssets = std::make_shared<std::vector<ExternalAsset>>();
    auto externalAssetsDef = gltfDef["externalAssets"];
    if (externalAssetsDef.is_array())
    {
        if (externalAssetsDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many externalAssets: " + std::to_string(externalAssetsDef.size()));
        }
        externalAssets->reserve(externalAssetsDef.size());
        for (uint32_t a = 0; a < externalAssetsDef.size(); a++)
        {
            auto extAsset = externalAssetsDef[a];
            auto name = extAsset.value("name", "");
            if (!extAsset.contains("file") || !extAsset.at("file").is_number())
                throw std::invalid_argument("ExternalAsset[" + std::to_string(a) + "] missing required 'file' field");
            int64_t fileIndex = extAsset.at("file").get<int64_t>();
            externalAssets->emplace_back(name, fileIndex);
            externalAssets->back().uid = parseUid(extAsset);
        }
    }

    // Nodes
    auto nodes = std::make_shared<std::vector<Node>>();
    auto nodesDef = gltfDef["nodes"];
    if (nodesDef.is_array())
    {
        if (nodesDef.size() > GLTF_MAX_ARRAY_ELEMENTS)
        {
            throw std::invalid_argument("Too many nodes: " + std::to_string(nodesDef.size()));
        }
        nodes->reserve(nodesDef.size());
        for (uint32_t a = 0; a < nodesDef.size(); a++)
        {
            auto nodeDef = nodesDef[a];

            auto name = nodeDef.value("name","");

            int64_t camera = nodeDef.value("camera", -1);

            std::vector<uint64_t> children;
            intListFromGLTF(children, nodeDef["children"]);

            int64_t skin = nodeDef.value("skin", -1);

            auto matrix = mat4FromGLTF(nodeDef["matrix"], systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE>::identity());

            int64_t mesh = nodeDef.value("mesh", -1);

            auto rotation = quatFromGLTF(nodeDef["rotation"], systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE>(0.0, 0.0, 0.0, 1.0));

            auto scale = vec3FromGLTF(nodeDef["scale"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(1.0, 1.0, 1.0));

            auto translation = vec3FromGLTF(nodeDef["translation"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(0.0, 0.0, 0.0));

            std::vector<GLTF_REAL_NUMBER_TYPE> weights;
            realListFromGLTF(weights, nodeDef["weights"]);

            bool visible = nodeDef.value("visible", true);

            int64_t nodeExternalAsset = nodeDef.value("externalAsset", -1);

            std::shared_ptr<BoundingVolume> nodeBoundingVolume = nullptr;
            auto boundingVolumeDef = nodeDef["boundingVolume"];
            if (boundingVolumeDef.is_object())
            {
                if (!boundingVolumeDef.contains("shape") || !boundingVolumeDef.at("shape").is_number())
                    throw std::invalid_argument("Node[" + std::to_string(a) + "] boundingVolume missing required 'shape' field");
                int64_t shapeIndex = boundingVolumeDef.at("shape").get<int64_t>();
                if (boundingVolumeDef.contains("matrix") && boundingVolumeDef.at("matrix").is_array())
                {
                    auto matrix = mat4FromGLTF(boundingVolumeDef["matrix"], systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE>::identity());
                    nodeBoundingVolume = std::make_shared<BoundingVolume>(shapeIndex, matrix);
                }
                else
                {
                    nodeBoundingVolume = std::make_shared<BoundingVolume>(shapeIndex);
                }
            }

            nlohmann::json emptyNodeExt = nlohmann::json::object();
            nlohmann::json& nodeExtDef = (nodeDef.contains("extensions") && nodeDef.at("extensions").is_object())
                ? nodeDef["extensions"] : emptyNodeExt;

            // KHR_node_visibility: promoted to core in glTF 2.1 as node.visible.
            // Continue to support the extension form for glTF 2.0 assets.
            if (!nodeDef.contains("visible") && nodeExtDef["KHR_node_visibility"].is_object())
            {
                visible = nodeExtDef["KHR_node_visibility"].value("visible", true);
            }

            // KHR_node_hoverability / KHR_node_selectability
            bool hoverable = nodeDef.value("hoverable", true);
            bool selectable = nodeDef.value("selectable", true);
            if (nodeExtDef["KHR_node_hoverability"].is_object())
            {
                hoverable = nodeExtDef["KHR_node_hoverability"].value("hoverable", true);
            }
            if (nodeExtDef["KHR_node_selectability"].is_object())
            {
                selectable = nodeExtDef["KHR_node_selectability"].value("selectable", true);
            }

            // EXT_lights_ies
            std::shared_ptr<EXTNodeLightsIES> extNodeLightsIes = nullptr;
            if (nodeExtDef["EXT_lights_ies"].is_object())
            {
                auto& d = nodeExtDef["EXT_lights_ies"];
                extNodeLightsIes = std::make_shared<EXTNodeLightsIES>(
                    d.value("light", int64_t(-1)),
                    d.value("multiplier", 1.0),
                    vec3FromGLTF(d["color"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(1, 1, 1)));
            }

            auto khrLightsPunctual = nodeExtDef["KHR_lights_punctual"];
            int64_t light = -1;
            if (khrLightsPunctual.is_object())
            {
                light = khrLightsPunctual.value("light", -1);
            }

            std::shared_ptr<EXTMeshGpuInstancing> extMeshGpuInstancing = nullptr;
            auto extMeshGpuInstancingDef = nodeExtDef["EXT_mesh_gpu_instancing"]["attributes"];
            if (extMeshGpuInstancingDef.is_object()) {
                extMeshGpuInstancing = std::make_shared<EXTMeshGpuInstancing>();
                for (auto &element : extMeshGpuInstancingDef.items()) {
                    extMeshGpuInstancing->attributes[element.key()] = element.value();
                }
            }

            int64_t nodeXmpPacket = -1;
            if (nodeExtDef["KHR_xmp_json_ld"].is_object())
            {
                nodeXmpPacket = nodeExtDef["KHR_xmp_json_ld"].value("packet", int64_t(-1));
            }

            nodes->emplace_back(
                name,
                camera,
                children,
                skin,
                matrix,
                mesh,
                rotation,
                scale,
                translation,
                weights,
                visible,
                light,
                extMeshGpuInstancing
            );
            nodes->back().khrXmpPacket = nodeXmpPacket;
            nodes->back().externalAsset = nodeExternalAsset;
            nodes->back().boundingVolume = nodeBoundingVolume;
            nodes->back().hoverable = hoverable;
            nodes->back().selectable = selectable;
            nodes->back().extLightsIes = extNodeLightsIes;
            nodes->back().uid = parseUid(nodeDef);
        }
    }

    // Scenes
    int64_t scene = gltfDef.value("scene", -1);
    auto scenes = std::make_shared<std::vector<Scene>>();
    auto scenesDef = gltfDef["scenes"];
    if (scenesDef.is_array())
    {
        scenes->reserve(scenesDef.size());
        for (uint32_t a = 0; a < scenesDef.size(); a++)
        {
            auto scene = scenesDef[a];
            auto name = scene.value("name","");
            scenes->emplace_back(name, uintListFromGLTF(scene["nodes"]));
            scenes->back().uid = parseUid(scene);

            if (scene.contains("extensions") && scene["extensions"]["EXT_lights_image_based"].is_object())
            {
                scenes->back().extLightImageBased = scene["extensions"]["EXT_lights_image_based"].value("light", int64_t(-1));
            }
        }
    }

    // Skins
    auto skins = std::make_shared<std::vector<Skin>>();
    auto skinsDef = gltfDef["skins"];
    if (skinsDef.is_array())
    {
        skins->reserve(skinsDef.size());
        for (uint32_t a = 0; a < skinsDef.size(); a++)
        {
            auto &skin = skinsDef[a];
            auto name = skin.value("name", "");
            int64_t inverseBindMatrices = skin.value("inverseBindMatrices", int64_t(-1));
            int64_t skeleton = skin.value("skeleton", int64_t(-1));
            auto joints = uintListFromGLTF(skin["joints"]);
            for (uint64_t joint : *joints)
                if (joint >= nodes->size())
                    throw std::invalid_argument("Skin joint index out of range");
            skins->emplace_back(name, inverseBindMatrices, skeleton, joints);
            skins->back().uid = parseUid(skin);
        }
    }

    // Animations
    auto animations = std::make_shared<std::vector<Animation>>();
    auto animationsDef = gltfDef["animations"];
    if (animationsDef.is_array())
    {
        animations->reserve(animationsDef.size());
        for (uint32_t a = 0; a < animationsDef.size(); a++)
        {
            auto animDef = animationsDef[a];
            auto animName = animDef.value("name", "");

            // Parse samplers
            std::vector<AnimationSampler> animSamplers;
            if (animDef.contains("samplers") && animDef["samplers"].is_array())
            {
                auto samplersDef = animDef["samplers"];
                animSamplers.reserve(samplersDef.size());
                for (auto &samplerDef : samplersDef)
                {
                    uint64_t input = samplerDef.value("input", 0);
                    uint64_t output = samplerDef.value("output", 0);
                    std::string interpStr = samplerDef.value("interpolation", "LINEAR");
                    AnimationInterpolation interp = AnimationInterpolation::aiLinear;
                    if (interpStr == "STEP")
                        interp = AnimationInterpolation::aiStep;
                    else if (interpStr == "CUBICSPLINE")
                        interp = AnimationInterpolation::aiCubicSpline;
                    animSamplers.push_back(AnimationSampler{input, interp, output});
                }
            }

            // Parse channels
            std::vector<AnimationChannel> animChannels;
            if (animDef.contains("channels") && animDef["channels"].is_array())
            {
                auto channelsDef = animDef["channels"];
                animChannels.reserve(channelsDef.size());
                for (auto &channelDef : channelsDef)
                {
                    uint64_t samplerIdx = channelDef.value("sampler", 0);
                    AnimationChannelTarget target;
                    if (channelDef.contains("target"))
                    {
                        auto &targetDef = channelDef["target"];
                        target.node = targetDef.value("node", int64_t(-1));
                        target.path = targetDef.value("path", "");
                        // KHR_animation_pointer
                        if (target.path == "pointer" &&
                            targetDef.contains("extensions") &&
                            targetDef["extensions"].is_object() &&
                            targetDef["extensions"].contains("KHR_animation_pointer"))
                        {
                            auto &ptrDef = targetDef["extensions"]["KHR_animation_pointer"];
                            if (ptrDef.contains("pointer"))
                                target.khrAnimationPointer = std::make_shared<KHRAnimationPointer>(ptrDef["pointer"].get<std::string>());
                        }
                    }
                    if (samplerIdx >= animSamplers.size())
                        throw std::invalid_argument("Animation channel sampler index out of range");
                    animChannels.push_back(AnimationChannel{samplerIdx, target});
                }
            }

            animations->emplace_back(animName, animChannels, animSamplers);
            animations->back().uid = parseUid(animDef);
        }
    }

    // KHR_lights_punctual
    auto khrLightsPunctual = std::make_shared<std::vector<KHRLightPunctual>>();
    auto khrLightsPunctualDef = gltfDef["extensions"]["KHR_lights_punctual"]["lights"];
    if (khrLightsPunctualDef.is_array())
    {
        khrLightsPunctual->reserve(khrLightsPunctualDef.size());
        for (uint32_t a = 0; a < khrLightsPunctualDef.size(); a++)
        {
            auto light = khrLightsPunctualDef[a];
            auto name = light.value("name","");
            if (!light.contains("type") || !light.at("type").is_string())
                throw std::invalid_argument("KHR_lights_punctual: light[" + std::to_string(a) + "] missing required 'type' field");
            std::string typeString = light.at("type").get<std::string>();
            KHRLightPunctualType type;
            if (typeString == "directional")
                type = KHRLightPunctualType::directional;
            else if (typeString == "point")
                type = KHRLightPunctualType::point;
            else if (typeString == "spot")
                type = KHRLightPunctualType::spot;
            else
            {
                throw std::invalid_argument("KHR_lights_punctual: unknown light type: " + typeString);
            }

            auto color = vec3FromGLTF(light["color"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(1, 1, 1));
            GLTF_REAL_NUMBER_TYPE intensity = light.value("intensity", 1);
            auto range = light.value("range", std::numeric_limits<GLTF_REAL_NUMBER_TYPE>::max());
            // Cone angles are nested inside the "spot" sub-object per spec.
            GLTF_REAL_NUMBER_TYPE innerConeAngle = 0;
            GLTF_REAL_NUMBER_TYPE outerConeAngle = M_PI_4;
            if (light.contains("spot") && light["spot"].is_object()) {
                innerConeAngle = light["spot"].value("innerConeAngle", 0.0);
                outerConeAngle = light["spot"].value("outerConeAngle", (double)M_PI_4);
            }

            khrLightsPunctual->emplace_back(
                name,
                type,
                color,
                intensity,
                range,
                innerConeAngle,
                outerConeAngle);
            khrLightsPunctual->back().uid = parseUid(light);
        }
    }

    // KHR_texture_procedurals
    auto khrTextureProcedurals = std::make_shared<KHRTextureProcedurals>();
    auto khrTextureProceduralsDef = gltfDef["extensions"]["KHR_texture_procedurals"];
    if (khrTextureProceduralsDef.is_object())
    {
        auto proceduralsDef = khrTextureProceduralsDef["procedurals"];
        if (proceduralsDef.is_array())
        {
            khrTextureProcedurals->procedurals.reserve(proceduralsDef.size());
            for (uint32_t a = 0; a < proceduralsDef.size(); ++a)
            {
                khrTextureProcedurals->procedurals.push_back(proceduralGraphFromGLTF(proceduralsDef[a]));
            }
        }
        auto definitionsDef = khrTextureProceduralsDef["procedural_definitions"];
        if (definitionsDef.is_array())
        {
            khrTextureProcedurals->proceduralDefinitions.reserve(definitionsDef.size());
            for (uint32_t a = 0; a < definitionsDef.size(); ++a)
            {
                khrTextureProcedurals->proceduralDefinitions.push_back(proceduralNodeDefFromGLTF(definitionsDef[a]));
            }
        }
    }

    // EXT_lights_ies
    auto extLightsIes = std::make_shared<std::vector<EXTLightIESProfile>>();
    auto extLightsIesDef = gltfDef["extensions"]["EXT_lights_ies"]["lights"];
    if (extLightsIesDef.is_array())
    {
        extLightsIes->reserve(extLightsIesDef.size());
        for (uint32_t a = 0; a < extLightsIesDef.size(); a++)
        {
            auto& light = extLightsIesDef[a];
            extLightsIes->emplace_back(light.value("name", ""));
            extLightsIes->back().uri = light.value("uri", "");
            extLightsIes->back().mimeType = light.value("mimeType", "");
            extLightsIes->back().bufferView = light.value("bufferView", int64_t(-1));
            extLightsIes->back().uid = parseUid(light);
        }
    }

    // EXT_lights_image_based
    auto extLightsImageBased = std::make_shared<std::vector<EXTLightImageBased>>();
    auto extLightsImageBasedDef = gltfDef["extensions"]["EXT_lights_image_based"]["lights"];
    if (extLightsImageBasedDef.is_array())
    {
        extLightsImageBased->reserve(extLightsImageBasedDef.size());
        for (uint32_t a = 0; a < extLightsImageBasedDef.size(); a++)
        {
            auto& light = extLightsImageBasedDef[a];
            extLightsImageBased->emplace_back(light.value("name", ""));
            extLightsImageBased->back().rotation = quatFromGLTF(
                light["rotation"],
                systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE>(0, 0, 0, 1));
            extLightsImageBased->back().intensity = light.value("intensity", 1.0);
            extLightsImageBased->back().specularImageSize = light.value("specularImageSize", uint64_t(0));

            auto& irrDef = light["irradianceCoefficients"];
            if (irrDef.is_array())
            {
                for (uint32_t i = 0; i < irrDef.size(); i++)
                {
                    std::array<GLTF_REAL_NUMBER_TYPE, 3> coeff{0, 0, 0};
                    if (irrDef[i].is_array() && irrDef[i].size() >= 3)
                    {
                        coeff[0] = irrDef[i][0];
                        coeff[1] = irrDef[i][1];
                        coeff[2] = irrDef[i][2];
                    }
                    extLightsImageBased->back().irradianceCoefficients.push_back(coeff);
                }
            }

            auto& specDef = light["specularImages"];
            if (specDef.is_array())
            {
                for (uint32_t i = 0; i < specDef.size(); i++)
                {
                    std::vector<uint64_t> faces;
                    if (specDef[i].is_array())
                    {
                        for (uint32_t f = 0; f < specDef[i].size(); f++)
                            faces.push_back(specDef[i][f]);
                    }
                    extLightsImageBased->back().specularImages.push_back(std::move(faces));
                }
            }
            extLightsImageBased->back().uid = parseUid(light);
        }
    }

    auto toReturn = std::shared_ptr<GLTF>(new GLTF(
        buffers,
        bufferViews,
        accessors,
        cameras,
        images,
        textures,
        scene,
        scenes,
        nodes,
        samplers,
        meshes,
        materials,
        animations,
        skins,
        khrLightsPunctual,
        khrTextureProcedurals,
        extLightsIes,
        extLightsImageBased));

    toReturn->files = files;
    toReturn->shapes = shapes;
    toReturn->externalAssets = externalAssets;
    toReturn->thumbnail = assetThumbnail;

    // Validate glTF 2.1 cross-references for new top-level arrays.
    if (assetThumbnail >= 0 && static_cast<uint64_t>(assetThumbnail) >= images->size())
    {
        throw std::invalid_argument("asset.thumbnail references out-of-range image index " + std::to_string(assetThumbnail));
    }

    for (uint32_t a = 0; a < files->size(); a++)
    {
        const auto &file = (*files)[a];
        if (file.bufferView != -1 && static_cast<uint64_t>(file.bufferView) >= bufferViews->size())
        {
            throw std::invalid_argument("File[" + std::to_string(a) + "] references out-of-range bufferView index " + std::to_string(file.bufferView));
        }
    }

    for (uint32_t a = 0; a < externalAssets->size(); a++)
    {
        int64_t fileIndex = (*externalAssets)[a].file;
        if (fileIndex < 0 || static_cast<uint64_t>(fileIndex) >= files->size())
        {
            throw std::invalid_argument("ExternalAsset[" + std::to_string(a) + "] references out-of-range file index " + std::to_string(fileIndex));
        }
    }

    for (uint32_t a = 0; a < nodes->size(); a++)
    {
        const auto &node = (*nodes)[a];
        if (node.externalAsset != -1 && static_cast<uint64_t>(node.externalAsset) >= externalAssets->size())
        {
            throw std::invalid_argument("Node[" + std::to_string(a) + "] references out-of-range externalAsset index " + std::to_string(node.externalAsset));
        }
        if (node.boundingVolume != nullptr)
        {
            if (node.boundingVolume->shape < 0 || static_cast<uint64_t>(node.boundingVolume->shape) >= shapes->size())
            {
                throw std::invalid_argument("Node[" + std::to_string(a) + "] boundingVolume references out-of-range shape index " + std::to_string(node.boundingVolume->shape));
            }
        }
    }

    // Validate mesh/primitive extension cross-references.
    for (uint32_t m = 0; m < meshes->size(); m++)
    {
        const auto &mesh = (*meshes)[m];
        for (uint32_t p = 0; p < mesh.primitives.size(); p++)
        {
            const auto &primitive = mesh.primitives[p];
            if (primitive.extMeshPrimitiveRestart != nullptr)
            {
                for (uint32_t g = 0; g < primitive.extMeshPrimitiveRestart->primitiveGroups.size(); g++)
                {
                    const auto &group = primitive.extMeshPrimitiveRestart->primitiveGroups[g];
                    for (auto primIdx : group.primitives)
                    {
                        if (primIdx >= mesh.primitives.size())
                        {
                            throw std::invalid_argument("EXT_mesh_primitive_restart group references out-of-range primitive index " + std::to_string(primIdx));
                        }
                    }
                }
            }
        }

        if (mesh.extMeshManifold != nullptr)
        {
            if (mesh.extMeshManifold->mergeIndices != -1 && static_cast<uint64_t>(mesh.extMeshManifold->mergeIndices) >= accessors->size())
            {
                throw std::invalid_argument("Mesh[" + std::to_string(m) + "] EXT_mesh_manifold mergeIndices references out-of-range accessor index " + std::to_string(mesh.extMeshManifold->mergeIndices));
            }
            if (mesh.extMeshManifold->mergeValues != -1 && static_cast<uint64_t>(mesh.extMeshManifold->mergeValues) >= accessors->size())
            {
                throw std::invalid_argument("Mesh[" + std::to_string(m) + "] EXT_mesh_manifold mergeValues references out-of-range accessor index " + std::to_string(mesh.extMeshManifold->mergeValues));
            }
        }
    }

    // KHR_materials_variants (top-level variants array)
    auto khrMaterialsVariantsDef = gltfDef["extensions"]["KHR_materials_variants"]["variants"];
    if (khrMaterialsVariantsDef.is_array())
    {
        toReturn->khrMaterialsVariants = std::make_shared<std::vector<KHRMaterialVariant>>();
        toReturn->khrMaterialsVariants->reserve(khrMaterialsVariantsDef.size());
        for (uint32_t a = 0; a < khrMaterialsVariantsDef.size(); a++)
        {
            toReturn->khrMaterialsVariants->emplace_back(
                khrMaterialsVariantsDef[a].value("name", ""));
        }
    }

    // KHR_xmp_json_ld (top-level packets)
    auto khrXmpPacketsDef = gltfDef["extensions"]["KHR_xmp_json_ld"]["packets"];
    if (khrXmpPacketsDef.is_array())
    {
        toReturn->khrXmpPackets = std::make_shared<std::vector<std::string>>();
        toReturn->khrXmpPackets->reserve(khrXmpPacketsDef.size());
        for (uint32_t a = 0; a < khrXmpPacketsDef.size(); a++)
        {
            toReturn->khrXmpPackets->push_back(khrXmpPacketsDef[a].dump());
        }
    }

    // KHR_xmp_json_ld (asset-level packet)
    auto assetXmpDef = gltfDef["asset"]["extensions"]["KHR_xmp_json_ld"];
    if (assetXmpDef.is_object())
    {
        toReturn->khrXmpAssetPacket = assetXmpDef.value("packet", int64_t(-1));
    }

    // KHR_draco_mesh_compression — decompress any primitives whose buffer data
    // is already available (inline base64). For GLB files the buffer is filled
    // later by loadGLB, which calls decompressDraco a second time.
    GLTF::decompressDraco(toReturn);

    // EXT_meshopt_compression — same pattern: decompress buffer views whose
    // data is already available. GLB calls this again after filling chunk 1.
    GLTF::decompressMeshopt(toReturn);

    // Validate glTF 2.1 unique identifiers.
    validateUids(
        *toReturn->buffers,
        *toReturn->bufferViews,
        *toReturn->accessors,
        *toReturn->cameras,
        *toReturn->images,
        *toReturn->textures,
        *toReturn->scenes,
        *toReturn->nodes,
        *toReturn->samplers,
        *toReturn->meshes,
        *toReturn->materials,
        *toReturn->animations,
        *toReturn->skins,
        toReturn->khrLightsPunctual ? *toReturn->khrLightsPunctual : std::vector<KHRLightPunctual>{},
        toReturn->extLightsIes ? *toReturn->extLightsIes : std::vector<EXTLightIESProfile>{},
        toReturn->extLightsImageBased ? *toReturn->extLightsImageBased : std::vector<EXTLightImageBased>{},
        toReturn->files ? *toReturn->files : std::vector<File>{},
        toReturn->shapes ? *toReturn->shapes : std::vector<std::shared_ptr<Shape>>{},
        toReturn->externalAssets ? *toReturn->externalAssets : std::vector<ExternalAsset>{});

    return toReturn;
}

std::shared_ptr<GLTF> GLTF::loadGLTF(
    const std::string &data,
    std::function<std::future<std::vector<uint8_t>>(const std::string &uri)> loadCallback)
{
    // First, parse the JSON without external resources
    auto gltf = loadGLTF(data);

    // Collect unique URIs that need external loading and map them to buffers/images
    std::unordered_set<std::string> uniqueUris;

    for (const auto &buffer : *gltf->buffers)
    {
        if (!buffer.uri.empty() && buffer.data.empty())
        {
            uniqueUris.insert(buffer.uri);
        }
    }

    for (const auto &image : *gltf->images)
    {
        if (!image.uri.empty() && image.data.empty())
        {
            uniqueUris.insert(image.uri);
        }
    }

    if (uniqueUris.empty())
    {
        // No external resources to load, decompression already done by loadGLTF
        return gltf;
    }

    // Launch async loading for all unique URIs
    std::unordered_map<std::string, std::future<std::vector<uint8_t>>> loadingFutures;
    for (const auto &uri : uniqueUris)
    {
        loadingFutures[uri] = loadCallback(uri);
    }

    // Wait for all futures to complete and collect results
    std::unordered_map<std::string, std::vector<uint8_t>> loadedData;
    for (auto &[uri, future] : loadingFutures)
    {
        loadedData[uri] = future.get();
    }

    // Assign loaded data to buffers
    for (auto &buffer : *gltf->buffers)
    {
        if (!buffer.uri.empty() && buffer.data.empty())
        {
            auto it = loadedData.find(buffer.uri);
            if (it != loadedData.end())
            {
                buffer.data = std::move(it->second);
            }
        }
    }

    // Assign loaded data to images
    for (auto &image : *gltf->images)
    {
        if (!image.uri.empty() && image.data.empty())
        {
            auto it = loadedData.find(image.uri);
            if (it != loadedData.end())
            {
                image.data = std::move(it->second);
            }
        }
    }

    // Now that external buffer data is available, decompress extensions
    GLTF::decompressDraco(gltf);
    GLTF::decompressMeshopt(gltf);

    return gltf;
}

std::string GLTF::toString()
{
    std::stringstream ss;
    ss << "{ " << "buffers: " << buffers->size() << ", ";
    ss << "bufferViews: " << bufferViews->size() << ", ";
    ss << "accessors: " << accessors->size() << ", ";
    ss << "cameras: " << cameras->size() << ", ";
    ss << "images: " << images->size() << ", ";
    ss << "textures: " << textures->size() << ", ";
    ss << "scene: " << scene << ", ";
    ss << "scenes: " << scenes->size() << ", ";
    ss << "nodes: " << nodes->size() << ", ";
    ss << "samplers: " << samplers->size() << ", ";
    ss << "meshes: " << meshes->size() << ", ";
    ss << "materials: " << materials->size() << ", ";
    ss << "animations: " << animations->size() << ", ";
    ss << "skins: " << skins->size() << ", ";
    ss << "files: " << (files ? std::to_string(files->size()) : "0") << ", ";
    ss << "shapes: " << (shapes ? std::to_string(shapes->size()) : "0") << ", ";
    ss << "externalAssets: " << (externalAssets ? std::to_string(externalAssets->size()) : "0") << ", ";
    ss << "thumbnail: " << thumbnail << ", ";
    ss << "khrLightsPunctual: " << khrLightsPunctual->size() << ", ";
    ss << "khrTextureProcedurals: " << (khrTextureProcedurals ? std::to_string(khrTextureProcedurals->procedurals.size()) : "0") << "}";
    return ss.str();
}

std::string systems::leal::gltf::getVersion()
{
    return std::to_string(gltf_VERSION_MAJOR) + "." + std::to_string(gltf_VERSION_MINOR) + "." + std::to_string(gltf_VERSION_PATCH);
}
