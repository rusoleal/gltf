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
static constexpr uint64_t GLTF_MAX_BUFFER_BYTE_LENGTH = 2ULL * 1024 * 1024 * 1024; // 2 GB


const std::array<char const *, 24> GLTF::implementedExtensions = {
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
    "KHR_texture_procedurals"
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
    std::shared_ptr<KHRTextureProcedurals> khrTextureProcedurals)
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
std::shared_ptr<GLTF> GLTF::loadGLB(uint8_t *data, uint64_t size)
{

    if (size < 12)
    {
        throw std::invalid_argument("Invalid glb size");
    }

    auto header = (uint32_t *)data;
    if (header[0] != 0x46546C67)
    {
        throw std::invalid_argument("Invalid GLB header");
    }

    if (header[1] != 2)
    {
        throw std::invalid_argument("Unsupported GLB version: " + std::to_string(header[1]));
    }

    if (header[2] != size)
    {
        throw std::invalid_argument("Wrong GLB size");
    }

    // chunk 0
    auto chunk0Length = header[3];
    auto chunk0Type = header[4];
    if (chunk0Type != CHUNK_TYPE_JSON)
    {
        throw std::invalid_argument("GLB chunk0 must be of type JSON");
    }
    auto chunk0 = std::string((char *)&header[5], chunk0Length);
    auto toReturn = loadGLTF(chunk0);

    // The start and the end of each chunk MUST be aligned to a 4-byte boundary.
    if (chunk0Length % 4 != 0)
    {
        chunk0Length += 4 - (chunk0Length % 4);
    }

    if (size > chunk0Length + 20)
    {
        auto chunk1Address = (uint32_t *)(((uint8_t *)&header[5]) + chunk0Length);
        auto chunk1Length = chunk1Address[0];
        auto chunk1Type = chunk1Address[1];
        if (chunk1Type == CHUNK_TYPE_BIN && toReturn->buffers->size() > 0)
        {
            auto &buffer0 = (*toReturn->buffers)[0];
            if (buffer0.byteLength != chunk1Length)
            {
                throw std::invalid_argument("GLB chunk1 incorrect length");
            }
            buffer0.data.resize(chunk1Length);
            memcpy(&(buffer0.data[0]), &(chunk1Address[2]), chunk1Length);

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
    if (parts[0] != "2")
    {
        throw std::invalid_argument("Check version issue. Specting version 2.x. Found version " + version);
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

            textures->emplace_back(name, sampler, source, extTextureWebp, khrTextureBasisuSource);
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
                auto primitiveDef = primitivesDef[a];

                if (!primitiveDef.contains("attributes") || !primitiveDef.at("attributes").is_object())
                    throw std::invalid_argument("Primitive[" + std::to_string(a) + "] missing required 'attributes' object");
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

                primitives.emplace_back(attributes, indices, material, mode, std::move(targets));

                nlohmann::json emptyPrimExt = nlohmann::json::object();
                nlohmann::json& primExtDef = (primitiveDef.contains("extensions") && primitiveDef.at("extensions").is_object())
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
                        primitives.back().khrMaterialsVariantsMappings.emplace_back(variantMaterial, std::move(variantIndices));
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
                    primitives.back().khrDracoMeshCompression =
                        std::make_shared<KHRDracoMeshCompression>(dracoBufferView, std::move(dracoAttribs));
                }
            }

            std::vector<GLTF_REAL_NUMBER_TYPE> weights;
            realListFromGLTF(weights, meshDef["weights"]);

            meshes->emplace_back(name, primitives, weights);

            nlohmann::json emptyMeshExt = nlohmann::json::object();
            nlohmann::json& meshExtDef = (meshDef.contains("extensions") && meshDef.at("extensions").is_object())
                ? meshDef["extensions"] : emptyMeshExt;
            if (meshExtDef["KHR_xmp_json_ld"].is_object())
            {
                meshes->back().khrXmpPacket = meshExtDef["KHR_xmp_json_ld"].value("packet", int64_t(-1));
            }
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

            nlohmann::json emptyNodeExt = nlohmann::json::object();
            nlohmann::json& nodeExtDef = (nodeDef.contains("extensions") && nodeDef.at("extensions").is_object())
                ? nodeDef["extensions"] : emptyNodeExt;

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
                light,
                extMeshGpuInstancing
            );
            nodes->back().khrXmpPacket = nodeXmpPacket;
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
        khrTextureProcedurals));

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
    ss << "khrLightsPunctual: " << khrLightsPunctual->size() << ", ";
    ss << "khrTextureProcedurals: " << (khrTextureProcedurals ? std::to_string(khrTextureProcedurals->procedurals.size()) : "0") << "}";
    return ss.str();
}

std::string systems::leal::gltf::getVersion()
{
    return std::to_string(gltf_VERSION_MAJOR) + "." + std::to_string(gltf_VERSION_MINOR) + "." + std::to_string(gltf_VERSION_PATCH);
}
