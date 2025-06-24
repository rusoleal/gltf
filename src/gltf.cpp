#include <exception>
#include <sstream>
#include <gltf/gltf.hpp>
#include "json.hpp"
#include "base64.h"
#include "utils.hpp"
#include "gltf_config.h"

using namespace systems::leal::gltf;
using json = nlohmann::json;

/*
// no inline, required by [replacement.functions]/3
void* operator new(std::size_t sz)
{
    std::printf("1) new(size_t), size = %zu\n", sz);
    if (sz == 0)
        ++sz; // avoid std::malloc(0) which may return nullptr on success

    if (void *ptr = std::malloc(sz))
        return ptr;

    throw std::bad_alloc{}; // required by [new.delete.single]/3
}

// no inline, required by [replacement.functions]/3
void* operator new[](std::size_t sz)
{
    std::printf("2) new[](size_t), size = %zu\n", sz);
    if (sz == 0)
        ++sz; // avoid std::malloc(0) which may return nullptr on success

    if (void *ptr = std::malloc(sz))
        return ptr;

    throw std::bad_alloc{}; // required by [new.delete.single]/3
}

void operator delete(void* ptr) noexcept
{
    std::puts("3) delete(void*)");
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept
{
    std::printf("4) delete(void*, size_t), size = %zu\n", size);
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    std::puts("5) delete[](void* ptr)");
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept
{
    std::printf("6) delete[](void*, size_t), size = %zu\n", size);
    std::free(ptr);
}
*/
const std::array<char const *, 2> GLTF::implementedExtensions = {
    "KHR_lights_punctual",
    "EXT_texture_webp"};

GLTF::GLTF(
    std::shared_ptr<std::vector<Buffer>> buffers,
    std::shared_ptr<std::vector<BufferView>> bufferViews,
    std::shared_ptr<std::vector<Accessor>> accessors,
    std::shared_ptr<std::vector<Camera>> cameras,
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
    std::shared_ptr<std::vector<KHRLightPunctual>> khrLightsPunctual)
{
    std::printf("GLTF::GLTF\n");
    this->buffers = buffers;
    this->bufferViews = bufferViews;
    this->accessors = accessors;
    this->cameras = cameras;
    this->images = images;
    this->textures = textures;
    this->scenes = scenes;
    this->nodes = nodes;
    this->samplers = samplers;
    this->meshes = meshes;
    this->materials = materials;
    this->animations = animations;
    this->skins = skins;
    this->khrLightsPunctual = khrLightsPunctual;
}

GLTF::~GLTF()
{
    std::printf("GLTF::~GLTF\n");
}

void GLTF::updateRuntimeInfoWithNode(std::shared_ptr<RuntimeInfo> runtimeInfo, Node &node)
{
    if (node.mesh != -1)
    {
        auto &mesh = (*meshes)[node.mesh];
        for (auto &primitive : mesh.primitives)
        {

            if (primitive.material != -1)
            {
                updateRuntimeInfoWithMaterial(runtimeInfo, (*materials)[primitive.material]);
            }

            // search for indices buffer
            if (primitive.indices != -1)
            {
                auto &accessor = (*accessors)[primitive.indices];
                if (accessor.bufferView != -1)
                {
                    auto &bufferView = (*bufferViews)[accessor.bufferView];
                    runtimeInfo->buffers[bufferView.buffer] = true;
                }
            }

            // search for vertex attributes
            for (auto const &[key, val] : primitive.attributes)
            {
                auto accessor = (*accessors)[val];
                if (accessor.bufferView != -1)
                {
                    auto &bufferView = (*bufferViews)[accessor.bufferView];
                    runtimeInfo->buffers[bufferView.buffer] = true;
                }
            }
        }
    }

    for (auto child : node.children)
    {
        updateRuntimeInfoWithNode(runtimeInfo, (*nodes)[child]);
    }
}

void GLTF::updateRuntimeInfoWithMaterial(std::shared_ptr<RuntimeInfo> runtimeInfo, Material &material)
{

    if (material.emissiveTexture != nullptr)
    {
        auto texture = (*textures)[material.emissiveTexture->index];
        if (texture.source != -1)
        {
            runtimeInfo->images[texture.source] = true;
        }
    }

    if (material.normalTexture != nullptr)
    {
        auto texture = (*textures)[material.normalTexture->index];
        if (texture.source != -1)
        {
            runtimeInfo->images[texture.source] = true;
        }
    }

    if (material.occlusionTexture != nullptr)
    {
        auto texture = (*textures)[material.occlusionTexture->index];
        if (texture.source != -1)
        {
            runtimeInfo->images[texture.source] = true;
        }
    }

    if (material.pbrMetallicRoughness != nullptr)
    {
        if (material.pbrMetallicRoughness->baseColorTexture != nullptr)
        {
            auto texture = (*textures)[material.pbrMetallicRoughness->baseColorTexture->index];
            if (texture.source != -1)
            {
                runtimeInfo->images[texture.source] = true;
            }
        }

        if (material.pbrMetallicRoughness->metallicRoughnessTexture != nullptr)
        {
            auto texture = (*textures)[material.pbrMetallicRoughness->metallicRoughnessTexture->index];
            if (texture.source != -1)
            {
                runtimeInfo->images[texture.source] = true;
            }
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

    auto &scene = (*scenes)[sceneIndex];
    for (auto nodeIndex : *scene.nodes)
    {
        updateRuntimeInfoWithNode(toReturn, (*nodes)[nodeIndex]);
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
        }
    }

    return toReturn;
}

std::shared_ptr<GLTF> GLTF::loadGLTF(const std::string &data)
{

    auto gltfDef = json::parse(data);

    // check version
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

        buffers->reserve(buffersDef.size());
        for (uint32_t a = 0; a < buffersDef.size(); a++)
        {
            auto buffer = buffersDef[a];

            auto name = buffer.value("name", "");

            uint64_t byteLength = buffer["byteLength"];

            std::vector<uint8_t> data = {};

            std::string uri = buffer.value("uri", "");
            if (startsWith(uri, "data:"))
            {
                // printf("%s",uri.c_str());
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

        bufferViews->reserve(bufferViewsDef.size());
        for (uint32_t a = 0; a < bufferViewsDef.size(); a++)
        {
            auto bufferView = bufferViewsDef[a];

            auto name = bufferView.value("name", "");
            uint64_t buffer = bufferView["buffer"];
            uint64_t byteOffset = bufferView.value("byteOffset", std::uint64_t(0));
            uint64_t byteLength = bufferView["byteLength"];
            int64_t byteStride = bufferView.value("byteStride", -1);
            BufferViewTarget target = (BufferViewTarget)bufferView.value("target", 0);
            bufferViews->emplace_back(name, buffer, byteOffset, byteLength, byteStride, target);
        }
    }

    // Accessors
    auto accessors = std::make_shared<std::vector<Accessor>>();
    auto accessorsDef = gltfDef["accessors"];
    if (accessorsDef.is_array())
    {

        accessors->reserve(accessorsDef.size());
        for (uint32_t a = 0; a < accessorsDef.size(); a++)
        {
            auto accessor = accessorsDef[a];

            auto name = accessor.value("name", "");
            int64_t bufferView = accessor.value("bufferView", -1);
            uint64_t byteOffset = accessor.value("byteOffset", std::uint64_t(0));
            ComponentType componentType = (ComponentType)(uint64_t)accessor["componentType"];
            bool normalized = accessor.value("normalized", false);
            uint64_t count = accessor["count"];
            std::string type = accessor["type"];
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
        }
    }

    // Cameras
    auto cameras = std::make_shared<std::vector<Camera>>();
    auto camerasDef = gltfDef["cameras"];
    if (camerasDef.is_array())
    {
        cameras->reserve(camerasDef.size());
        for (uint32_t a = 0; a < camerasDef.size(); a++)
        {
            auto camera = camerasDef[a];
            std::string name = camera.value("name", "");
            std::string type = camera["type"];
            if (type == "perspective")
            {

                double *aspectRatio = nullptr;
                if (camera["perspective"]["aspectRatio"].is_number())
                {
                    aspectRatio = new double(camera["perspective"]["aspectRatio"]);
                }
                double yfov = camera["perspective"]["yfov"];
                double *zfar = nullptr;
                if (camera["perspective"]["zfar"].is_number())
                {
                    aspectRatio = new double(camera["perspective"]["zfar"]);
                }
                double znear = camera["perspective"]["znear"];
                cameras->emplace_back(PerspectiveCamera(name, aspectRatio, yfov, zfar, znear));
            }
            else if (type == "orthographic")
            {
                double xmag = camera["orthographic"]["xmag"];
                double ymag = camera["orthographic"]["ymag"];
                double zfar = camera["orthographic"]["zfar"];
                double znear = camera["orthographic"]["znear"];
                cameras->emplace_back(OrthographicCamera(name, xmag, ymag, zfar, znear));
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
            int64_t extTextureWebp = texture["extensions"]["EXT_texture_webp"].value("source", -1);

            textures->emplace_back(name, sampler, source, extTextureWebp);
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
                normalTexture = std::make_shared<NormalTextureInfo>(
                    normalTextureDef["index"],
                    normalTextureDef.value("texCoord", 0),
                    normalTextureDef.value("scale", 1));
            }

            auto occlusionTextureDef = materialDef["occlusionTexture"];
            std::shared_ptr<OcclusionTextureInfo> occlusionTexture = nullptr;
            if (occlusionTextureDef.is_object())
            {
                occlusionTexture = std::make_shared<OcclusionTextureInfo>(
                    occlusionTextureDef["index"],
                    occlusionTextureDef.value("texCoord", 0),
                    occlusionTextureDef.value("strength", 1));
            }

            auto emissiveTexture = textureInfoFromGLTF(materialDef["emissiveTexture"]);

            auto emissiveFactor = vec3FromGLTF(materialDef["emissiveFactor"], systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(0, 0, 0));

            auto alphaMode = alphaModeFromGLTF(materialDef["alphaMode"]);

            GLTF_REAL_NUMBER_TYPE alphaCutoff = materialDef.value("alphaCutoff", 0.5);

            bool doubleSided = materialDef.value("doubleSided", false);

            materials->emplace_back(
                name,
                pbrMetallicRoughness,
                normalTexture,
                occlusionTexture,
                emissiveTexture,
                emissiveFactor,
                alphaMode,
                alphaCutoff,
                doubleSided);
        }
    }

    // Meshes
    auto meshes = std::make_shared<std::vector<Mesh>>();
    auto meshesDef = gltfDef["meshes"];
    if (meshesDef.is_array())
    {
        meshes->reserve(meshesDef.size());
        for (uint32_t a = 0; a < meshesDef.size(); a++)
        {
            auto meshDef = meshesDef[a];

            auto name = meshDef.value("name","");

            auto primitivesDef = meshDef["primitives"];
            std::vector<Primitive> primitives;
            for (int a = 0; a < primitivesDef.size(); a++)
            {
                auto primitiveDef = primitivesDef[a];

                auto attributesDef = primitiveDef["attributes"];
                std::unordered_map<std::string, uint64_t> attributes;
                for (auto &element : attributesDef.items())
                {
                    attributes[element.key()] = element.value();
                }

                int64_t indices = primitiveDef.value("indices", -1);

                int64_t material = primitiveDef.value("material", -1);

                PrimitiveMode mode = (PrimitiveMode)primitiveDef.value("mode", 4);

                // TODO load targets

                primitives.emplace_back(attributes, indices, material, mode, std::vector<uint8_t>());
            }

            std::vector<GLTF_REAL_NUMBER_TYPE> weights;
            realListFromGLTF(weights, meshDef["weights"]);

            meshes->emplace_back(name, primitives, weights);
        }
    }

    // Nodes
    auto nodes = std::make_shared<std::vector<Node>>();
    auto nodesDef = gltfDef["nodes"];
    if (nodesDef.is_array())
    {
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

            auto khrLightsPunctual = nodeDef["extensions"]["KHR_lights_punctual"];
            int64_t light = -1;
            if (khrLightsPunctual.is_object())
            {
                light = khrLightsPunctual.value("light", -1);
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
                light);
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
            auto skin = skinsDef[a];
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
            auto animation = animationsDef[a];
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
            std::string typeString = light["type"];
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
            GLTF_REAL_NUMBER_TYPE innerConeAngle = light.value("innerConeAngle", 0);
            GLTF_REAL_NUMBER_TYPE outerConeAngle = light.value("outerConeAngle", M_PI_4);

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

    return std::shared_ptr<GLTF>(new GLTF(
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
        khrLightsPunctual));
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
    ss << "khrLightsPunctual: " << khrLightsPunctual->size() << "}";
    return ss.str();
}

std::string systems::leal::gltf::getVersion()
{
    return std::to_string(gltf_VERSION_MAJOR) + "." + std::to_string(gltf_VERSION_MINOR) + "." + std::to_string(gltf_VERSION_PATCH);
}
