#include <exception>
#include <gltf/gltf.hpp>
#include <nlohmann/json.hpp>

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
const std::array<char const *, 1> GLTF::implementedExtensions = {
    "KHR_lights_punctual",
};

GLTF::GLTF(
    std::shared_ptr<std::vector<Buffer>> buffers,
    std::shared_ptr<std::vector<BufferView>> bufferViews,
    std::shared_ptr<std::vector<Accessor>> accessors,
    std::shared_ptr<std::vector<Camera>> cameras,
    std::shared_ptr<std::vector<Image>> images,
    std::shared_ptr<std::vector<Texture>> textures,
    std::shared_ptr<std::vector<Scene>> scenes,
    std::shared_ptr<std::vector<Node>> nodes,
    std::shared_ptr<std::vector<Sampler>> samplers,
    std::shared_ptr<std::vector<Mesh>> meshes,
    std::shared_ptr<std::vector<Material>> materials,
    std::shared_ptr<std::vector<Animation>> animations,
    std::shared_ptr<std::vector<Skin>> skins,
    std::shared_ptr<std::vector<KHRLightPunctual>> khrLightsPunctual
) {
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

GLTF::~GLTF() {
    std::printf("GLTF::~GLTF\n");
}


FilterMode *filterModeFromGLTF(nlohmann::json &node) {
    if (node.is_number()) {
        return new FilterMode((FilterMode)(uint64_t)node);
    }
    return nullptr;
}

std::vector<uint64_t> *uintListFromGLTF(nlohmann::json &node) {
    if (node.is_array()) {
        auto toReturn = new std::vector<uint64_t>();
        toReturn->reserve(node.size());
        for (uint64_t a=0; a<node.size(); a++) {
            toReturn->emplace_back(node[a]);
        }
        return toReturn;
    }
    return nullptr;
}

std::shared_ptr<GLTF> GLTF::loadGLTF(const std::string &data, onLoadDataEvent onLoadData) {

    auto gltfDef = json::parse(data);

    // check version
    std::string version = gltfDef["asset"]["version"];

    
    // check implemented versions
    
    
    // Buffers
    auto buffers = std::make_shared<std::vector<Buffer>>();
    auto buffersDef = gltfDef["buffers"];
    if (buffersDef.is_array()) {
        
        buffers->reserve(buffersDef.size());
        for (uint32_t a=0; a<buffersDef.size(); a++ ) {
            auto buffer = buffersDef[a];
            
            std::string *uri = nullptr;
            if (buffer["uri"].is_string()) {
                uri = new std::string(buffer["uri"]);
            }

            uint64_t byteLength = buffer["byteLength"];

            buffers->emplace_back((Buffer){uri, byteLength});

        }
    }

    // BufferViews
    auto bufferViews = std::make_shared<std::vector<BufferView>>();
    auto bufferViewsDef = gltfDef["bufferViews"];
    if (bufferViewsDef.is_array()) {
        
        bufferViews->reserve(bufferViewsDef.size());
        for (uint32_t a=0; a<bufferViewsDef.size(); a++ ) {
            auto bufferView = bufferViewsDef[a];
            
            uint64_t buffer = bufferView["buffer"];
            uint64_t byteOffset = bufferView.value("byteOffset", std::uint64_t(0));
            uint64_t byteLength = bufferView["byteLength"];
            uint64_t *byteStride = nullptr;
            if (bufferView["byteStride"].is_number()) {
                byteStride = new uint64_t(bufferView["byteStride"]);
            }
            BufferViewTarget *target = nullptr;
            if (bufferView["target"].is_number()) {
                target = new BufferViewTarget(bufferView["target"]);
            }
            bufferViews->emplace_back(buffer, byteOffset, byteLength, byteStride, target);
        }
    }

    // Accessors
    auto accessors = std::make_shared<std::vector<Accessor>>();
    auto accessorsDef = gltfDef["accessors"];
    if (accessorsDef.is_array()) {
        
        accessors->reserve(accessorsDef.size());
        for (uint32_t a=0; a<accessorsDef.size(); a++ ) {
            auto accessor = accessorsDef[a];
            
            uint64_t *bufferView = nullptr;
            if (accessor["bufferView"].is_number()) {
                bufferView = new uint64_t(accessor["bufferView"]);
            }
            uint64_t byteOffset = accessor.value("byteOffset", std::uint64_t(0));
            ComponentType componentType = (ComponentType)(uint64_t)accessor["componentType"];
            bool normalized = accessor.value("normalized", false);
            uint64_t count = accessor["count"];
            std::string type = accessor["type"];
            AccessorType t;
            if (type == "SCALAR") {
                t = AccessorType::acScalar;
            } else if (type == "VEC2") {
                t = AccessorType::acVec2;
            } else if (type == "VEC3") {
                t = AccessorType::acVec3;
            } else if (type == "VEC4") {
                t = AccessorType::acVec4;
            } else if (type == "MAT2") {
                t = AccessorType::acMat2;
            } else if (type == "MAT3") {
                t = AccessorType::acMat3;
            } else if (type == "MAT4") {
                t = AccessorType::acMat4;
            } else {
                throw std::invalid_argument("Unsupported accessor type");
            }

            accessors->emplace_back(bufferView, byteOffset, componentType, normalized, count, t);
        }
    }

    // Cameras
    auto cameras = std::make_shared<std::vector<Camera>>();
    auto camerasDef = gltfDef["cameras"];
    if (camerasDef.is_array()) {
        cameras->reserve(camerasDef.size());
        for (uint32_t a=0; a<camerasDef.size(); a++) {
            auto camera = camerasDef[a];
            std::string type = camera["type"];
            if (type == "perspective") {
                
                double *aspectRatio = nullptr;
                if (camera["perspective"]["aspectRatio"].is_number()) {
                    aspectRatio = new double(camera["perspective"]["aspectRatio"]);
                }
                double yfov = camera["perspective"]["yfov"];
                double *zfar = nullptr;
                if (camera["perspective"]["zfar"].is_number()) {
                    aspectRatio = new double(camera["perspective"]["zfar"]);
                }
                double znear = camera["perspective"]["znear"];
                cameras->emplace_back(PerspectiveCamera(aspectRatio, yfov, zfar, znear));

            } else if (type == "orthographic") {
                double xmag = camera["orthographic"]["xmag"];
                double ymag = camera["orthographic"]["ymag"];
                double zfar = camera["orthographic"]["zfar"];
                double znear = camera["orthographic"]["znear"];
                cameras->emplace_back(OrthographicCamera(xmag, ymag, zfar, znear));

            } else {
                throw std::invalid_argument("Unsupported camera type");
            }
        }
    }

    // Images
    auto images = std::make_shared<std::vector<Image>>();
    auto imagesDef = gltfDef["images"];
    if (imagesDef.is_array()) {
        images->reserve(imagesDef.size());
        for (uint32_t a=0; a<imagesDef.size(); a++) {
            auto image = imagesDef[a];

            std::string *uri = nullptr;
            if (image["uri"].is_string()) {
                uri = new std::string(image["uri"]);
            }
            std::string *mimeType = nullptr;
            if (image["mimeType"].is_string()) {
                uri = new std::string(image["mimeType"]);
            }
            uint64_t *bufferView = nullptr;
            if (image["bufferView"].is_number()) {
                bufferView = new uint64_t(image["bufferView"]);
            }
            images->emplace_back(uri, mimeType, bufferView);

        }
    }

    // Textures
    auto textures = std::make_shared<std::vector<Texture>>();
    auto texturesDef = gltfDef["textures"];
    if (texturesDef.is_array()) {
        textures->reserve(texturesDef.size());
        for (uint32_t a=0; a<texturesDef.size(); a++) {
            auto texture = texturesDef[a];

            uint64_t *sampler = nullptr;
            if (texture["sampler"].is_number()) {
                sampler = new uint64_t(texture["sampler"]);
            }
            uint64_t *source = nullptr;
            if (texture["source"].is_number()) {
                source = new uint64_t(texture["source"]);
            }
            textures->emplace_back(sampler, source);
        }
    }

    // Samplers
    auto samplers = std::make_shared<std::vector<Sampler>>();
    auto samplersDef = gltfDef["samplers"];
    if (samplersDef.is_array()) {
        samplers->reserve(samplersDef.size());
        for (uint32_t a=0; a<samplersDef.size(); a++) {
            auto sampler = samplersDef[a];

            FilterMode *magFilter = filterModeFromGLTF(sampler["magFilter"]);
            FilterMode *minFilter = filterModeFromGLTF(sampler["minFilter"]);
            WrapMode wrapS = (WrapMode)sampler.value("wrapS",uint64_t(10497));
            WrapMode wrapT = (WrapMode)sampler.value("wrapT",uint64_t(10497));

            samplers->emplace_back(magFilter, minFilter, wrapS, wrapT);
        }
    }

    // Materials
    auto materials = std::make_shared<std::vector<Material>>();
    auto materialsDef = gltfDef["materials"];
    if (materialsDef.is_array()) {
        materials->reserve(materialsDef.size());
        for (uint32_t a=0; a<materialsDef.size(); a++) {
            auto material = materialsDef[a];
        }
    }

    // Meshes
    auto meshes = std::make_shared<std::vector<Mesh>>();
    auto meshesDef = gltfDef["meshes"];
    if (meshesDef.is_array()) {
        meshes->reserve(meshesDef.size());
        for (uint32_t a=0; a<meshesDef.size(); a++) {
            auto mesh = meshesDef[a];
        }
    }

    // Nodes
    auto nodes = std::make_shared<std::vector<Node>>();
    auto nodesDef = gltfDef["nodes"];
    if (nodesDef.is_array()) {
        nodes->reserve(nodesDef.size());
        for (uint32_t a=0; a<nodesDef.size(); a++) {
            auto node = nodesDef[a];
        }
    }

    // Scenes
    auto scenes = std::make_shared<std::vector<Scene>>();
    auto scenesDef = gltfDef["scenes"];
    if (scenesDef.is_array()) {
        scenes->reserve(scenesDef.size());
        for (uint32_t a=0; a<scenesDef.size(); a++) {
            auto scene = scenesDef[a];
            scenes->emplace_back(uintListFromGLTF(scene["nodes"]));
        }
    }

    // Skins
    auto skins = std::make_shared<std::vector<Skin>>();
    auto skinsDef = gltfDef["skins"];
    if (skinsDef.is_array()) {
        skins->reserve(skinsDef.size());
        for (uint32_t a=0; a<skinsDef.size(); a++) {
            auto skin = skinsDef[a];
        }
    }

    // Animations
    auto animations = std::make_shared<std::vector<Animation>>();
    auto animationsDef = gltfDef["animations"];
    if (animationsDef.is_array()) {
        animations->reserve(animationsDef.size());
        for (uint32_t a=0; a<animationsDef.size(); a++) {
            auto animation = animationsDef[a];
        }
    }

    // KHR_lights_punctual
    auto khrLightsPunctual = std::make_shared<std::vector<KHRLightPunctual>>();
    
    return std::shared_ptr<GLTF>(new GLTF(
        buffers,
        bufferViews,
        accessors,
        cameras,
        images,
        textures,
        scenes,
        nodes,
        samplers,
        meshes,
        materials,
        animations,
        skins,
        khrLightsPunctual
    ));
}

std::string GLTF::toString() {
    return "";
}

