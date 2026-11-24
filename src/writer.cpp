#include <gltf/writer.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "json.hpp"

namespace systems::leal::gltf
{

    namespace writer_detail
    {
        using OJson = nlohmann::ordered_json;

        const char *typeName(AccessorType type)
        {
            switch (type)
            {
            case AccessorType::acScalar: return "SCALAR";
            case AccessorType::acVec2: return "VEC2";
            case AccessorType::acVec3: return "VEC3";
            case AccessorType::acVec4: return "VEC4";
            case AccessorType::acMat2: return "MAT2";
            case AccessorType::acMat3: return "MAT3";
            case AccessorType::acMat4: return "MAT4";
            }
            throw std::invalid_argument("GLTFWriter: unknown accessor type");
        }

        uint32_t components(AccessorType type)
        {
            switch (type)
            {
            case AccessorType::acScalar: return 1;
            case AccessorType::acVec2: return 2;
            case AccessorType::acVec3: return 3;
            case AccessorType::acVec4: return 4;
            case AccessorType::acMat2: return 4;
            case AccessorType::acMat3: return 9;
            case AccessorType::acMat4: return 16;
            }
            return 0;
        }

        const char *alphaModeName(AlphaMode mode)
        {
            switch (mode)
            {
            case AlphaMode::mask: return "MASK";
            case AlphaMode::blend: return "BLEND";
            default: return "OPAQUE";
            }
        }

        // A finite number for JSON (NaN and infinity aren't valid JSON numbers).
        double finite(double value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("GLTFWriter: non-finite number");
            return value;
        }

        void checkIndex(int32_t index, size_t count, const char *what)
        {
            if (index < 0 || size_t(index) >= count)
                throw std::out_of_range(std::string("GLTFWriter: invalid ") + what + " index " +
                                        std::to_string(index));
        }

        void put32(std::vector<uint8_t> &out, uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(uint8_t(value >> (8 * i)));
        }
    }

    using namespace writer_detail;

    struct GLTFWriter::State
    {
        std::string generator;
        std::vector<uint8_t> bin;
        OJson bufferViews = OJson::array();
        OJson accessors = OJson::array();
        OJson images = OJson::array();
        OJson samplers = OJson::array();
        OJson textures = OJson::array();
        OJson materials = OJson::array();
        OJson meshes = OJson::array();
        OJson nodes = OJson::array();
        OJson scenes = OJson::array();
        int32_t defaultScene = -1;
        std::vector<std::string> extensionsUsed, extensionsRequired;
    };

    GLTFWriter::GLTFWriter(std::string generator) : state(std::make_unique<State>())
    {
        state->generator = std::move(generator);
    }

    GLTFWriter::~GLTFWriter() = default;

    int32_t GLTFWriter::addBufferView(const void *data, uint64_t byteLength, BufferViewTarget target,
                                      uint32_t byteStride)
    {
        if (byteLength == 0)
            throw std::invalid_argument("GLTFWriter: empty buffer view");
        if (byteStride != 0 && (byteStride < 4 || byteStride > 252 || byteStride % 4 != 0))
            throw std::invalid_argument("GLTFWriter: byteStride must be a multiple of 4 in [4, 252]");
        auto &bin = state->bin;
        bin.resize((bin.size() + 3) & ~size_t(3), 0);
        const uint64_t offset = bin.size();
        const auto *bytes = static_cast<const uint8_t *>(data);
        bin.insert(bin.end(), bytes, bytes + byteLength);

        OJson view = {{"buffer", 0}, {"byteOffset", offset}, {"byteLength", byteLength}};
        if (byteStride)
            view["byteStride"] = byteStride;
        if (target != BufferViewTarget::undefined)
            view["target"] = uint32_t(target);
        state->bufferViews.push_back(std::move(view));
        return int32_t(state->bufferViews.size() - 1);
    }

    int32_t GLTFWriter::addAccessor(int32_t bufferView, ComponentType componentType, AccessorType type,
                                    uint64_t count, bool normalized, uint64_t byteOffset,
                                    const std::vector<double> &min, const std::vector<double> &max)
    {
        checkIndex(bufferView, state->bufferViews.size(), "bufferView");
        if (count == 0)
            throw std::invalid_argument("GLTFWriter: accessor count must be at least 1");
        const size_t n = components(type);
        if ((!min.empty() && min.size() != n) || (!max.empty() && max.size() != n))
            throw std::invalid_argument("GLTFWriter: accessor min / max must have one value per component");

        OJson accessor = {{"bufferView", bufferView}};
        if (byteOffset)
            accessor["byteOffset"] = byteOffset;
        accessor["componentType"] = uint32_t(componentType);
        if (normalized)
            accessor["normalized"] = true;
        accessor["count"] = count;
        accessor["type"] = typeName(type);
        if (!min.empty())
        {
            OJson values = OJson::array();
            for (double v : min)
                values.push_back(finite(v));
            accessor["min"] = std::move(values);
        }
        if (!max.empty())
        {
            OJson values = OJson::array();
            for (double v : max)
                values.push_back(finite(v));
            accessor["max"] = std::move(values);
        }
        state->accessors.push_back(std::move(accessor));
        return int32_t(state->accessors.size() - 1);
    }

    int32_t GLTFWriter::addFloatAttribute(const float *data, uint64_t count, AccessorType type, bool bounds)
    {
        const uint32_t n = components(type);
        if (n == 0 || n > 4)
            throw std::invalid_argument("GLTFWriter: vertex attributes are SCALAR, VEC2, VEC3 or VEC4");
        std::vector<double> lo, hi;
        if (bounds)
        {
            lo.assign(n, std::numeric_limits<double>::max());
            hi.assign(n, std::numeric_limits<double>::lowest());
            for (uint64_t i = 0; i < count; ++i)
                for (uint32_t c = 0; c < n; ++c)
                {
                    const double v = data[i * n + c];
                    lo[c] = std::min(lo[c], v);
                    hi[c] = std::max(hi[c], v);
                }
        }
        const int32_t view =
            addBufferView(data, count * n * sizeof(float), BufferViewTarget::arrayBuffer);
        return addAccessor(view, ComponentType::ctFloat, type, count, false, 0, lo, hi);
    }

    int32_t GLTFWriter::addPositions(const float *xyz, uint64_t count)
    {
        return addFloatAttribute(xyz, count, AccessorType::acVec3, true);
    }

    int32_t GLTFWriter::addColors(const uint8_t *data, uint64_t count, AccessorType type)
    {
        if (type != AccessorType::acVec3 && type != AccessorType::acVec4)
            throw std::invalid_argument("GLTFWriter: colors are VEC3 or VEC4");
        const uint32_t n = components(type);
        // Vertex attributes must start each element on a 4-byte boundary:
        // RGB bytes are padded to a stride of 4.
        if (n == 3)
        {
            std::vector<uint8_t> padded(count * 4, 0);
            for (uint64_t i = 0; i < count; ++i)
                std::memcpy(&padded[i * 4], &data[i * 3], 3);
            const int32_t view = addBufferView(padded.data(), padded.size(), BufferViewTarget::arrayBuffer, 4);
            return addAccessor(view, ComponentType::ctUnsignedByte, type, count, true);
        }
        const int32_t view = addBufferView(data, count * 4, BufferViewTarget::arrayBuffer);
        return addAccessor(view, ComponentType::ctUnsignedByte, type, count, true);
    }

    int32_t GLTFWriter::addIndices(const uint32_t *indices, uint64_t count)
    {
        uint32_t largest = 0;
        for (uint64_t i = 0; i < count; ++i)
            largest = std::max(largest, indices[i]);
        if (largest == std::numeric_limits<uint32_t>::max())
            throw std::invalid_argument("GLTFWriter: index 0xFFFFFFFF is reserved");
        if (largest < 0xFFFFu)
        {
            std::vector<uint16_t> narrow(indices, indices + count);
            const int32_t view = addBufferView(narrow.data(), count * sizeof(uint16_t),
                                               BufferViewTarget::elementArrayBuffer);
            return addAccessor(view, ComponentType::ctUnsignedShort, AccessorType::acScalar, count);
        }
        const int32_t view =
            addBufferView(indices, count * sizeof(uint32_t), BufferViewTarget::elementArrayBuffer);
        return addAccessor(view, ComponentType::ctUnsignedInt, AccessorType::acScalar, count);
    }

    int32_t GLTFWriter::addImage(const std::vector<uint8_t> &encoded, const std::string &mimeType,
                                 const std::string &name)
    {
        const int32_t view = addBufferView(encoded.data(), encoded.size());
        OJson image;
        if (!name.empty())
            image["name"] = name;
        image["bufferView"] = view;
        image["mimeType"] = mimeType;
        state->images.push_back(std::move(image));
        return int32_t(state->images.size() - 1);
    }

    int32_t GLTFWriter::addSampler(FilterMode magFilter, FilterMode minFilter, WrapMode wrapS, WrapMode wrapT)
    {
        OJson sampler = OJson::object();
        if (magFilter != FilterMode::fmUnknown)
            sampler["magFilter"] = uint32_t(magFilter);
        if (minFilter != FilterMode::fmUnknown)
            sampler["minFilter"] = uint32_t(minFilter);
        sampler["wrapS"] = uint32_t(wrapS);
        sampler["wrapT"] = uint32_t(wrapT);
        state->samplers.push_back(std::move(sampler));
        return int32_t(state->samplers.size() - 1);
    }

    int32_t GLTFWriter::addTexture(int32_t image, int32_t sampler, bool basisu)
    {
        checkIndex(image, state->images.size(), "image");
        OJson texture = OJson::object();
        if (sampler >= 0)
        {
            checkIndex(sampler, state->samplers.size(), "sampler");
            texture["sampler"] = sampler;
        }
        if (basisu)
        {
            texture["extensions"] = {{"KHR_texture_basisu", {{"source", image}}}};
            useExtension("KHR_texture_basisu", true);
        }
        else
        {
            texture["source"] = image;
        }
        state->textures.push_back(std::move(texture));
        return int32_t(state->textures.size() - 1);
    }

    int32_t GLTFWriter::addMaterial(const WriterMaterial &m)
    {
        auto textureInfo = [&](int32_t texture) {
            checkIndex(texture, state->textures.size(), "texture");
            return OJson{{"index", texture}};
        };
        OJson material;
        if (!m.name.empty())
            material["name"] = m.name;
        OJson pbr = OJson::object();
        if (m.baseColorFactor != std::array<double, 4>{1.0, 1.0, 1.0, 1.0})
            pbr["baseColorFactor"] = {finite(m.baseColorFactor[0]), finite(m.baseColorFactor[1]),
                                      finite(m.baseColorFactor[2]), finite(m.baseColorFactor[3])};
        if (m.baseColorTexture >= 0)
            pbr["baseColorTexture"] = textureInfo(m.baseColorTexture);
        if (m.metallicFactor != 1.0)
            pbr["metallicFactor"] = finite(m.metallicFactor);
        if (m.roughnessFactor != 1.0)
            pbr["roughnessFactor"] = finite(m.roughnessFactor);
        if (m.metallicRoughnessTexture >= 0)
            pbr["metallicRoughnessTexture"] = textureInfo(m.metallicRoughnessTexture);
        material["pbrMetallicRoughness"] = std::move(pbr);
        if (m.normalTexture >= 0)
        {
            OJson normal = textureInfo(m.normalTexture);
            if (m.normalScale != 1.0)
                normal["scale"] = finite(m.normalScale);
            material["normalTexture"] = std::move(normal);
        }
        if (m.occlusionTexture >= 0)
            material["occlusionTexture"] = textureInfo(m.occlusionTexture);
        if (m.emissiveTexture >= 0)
            material["emissiveTexture"] = textureInfo(m.emissiveTexture);
        if (m.emissiveFactor != std::array<double, 3>{0.0, 0.0, 0.0})
            material["emissiveFactor"] = {finite(m.emissiveFactor[0]), finite(m.emissiveFactor[1]),
                                          finite(m.emissiveFactor[2])};
        if (m.alphaMode != AlphaMode::opaque)
            material["alphaMode"] = alphaModeName(m.alphaMode);
        if (m.alphaMode == AlphaMode::mask && m.alphaCutoff != 0.5)
            material["alphaCutoff"] = finite(m.alphaCutoff);
        if (m.doubleSided)
            material["doubleSided"] = true;
        if (m.unlit)
        {
            material["extensions"] = {{"KHR_materials_unlit", OJson::object()}};
            useExtension("KHR_materials_unlit");
        }
        state->materials.push_back(std::move(material));
        return int32_t(state->materials.size() - 1);
    }

    int32_t GLTFWriter::addMesh(const WriterMesh &mesh)
    {
        if (mesh.primitives.empty())
            throw std::invalid_argument("GLTFWriter: a mesh needs at least one primitive");
        OJson primitives = OJson::array();
        for (const auto &p : mesh.primitives)
        {
            if (p.attributes.empty())
                throw std::invalid_argument("GLTFWriter: a primitive needs at least one attribute");
            OJson attributes = OJson::object();
            for (const auto &[semantic, accessor] : p.attributes)
            {
                checkIndex(accessor, state->accessors.size(), "accessor");
                attributes[semantic] = accessor;
            }
            OJson primitive = {{"attributes", std::move(attributes)}};
            if (p.indices >= 0)
            {
                checkIndex(p.indices, state->accessors.size(), "accessor");
                primitive["indices"] = p.indices;
            }
            if (p.material >= 0)
            {
                checkIndex(p.material, state->materials.size(), "material");
                primitive["material"] = p.material;
            }
            if (p.mode != PrimitiveMode::pmTriangles)
                primitive["mode"] = uint32_t(p.mode);
            primitives.push_back(std::move(primitive));
        }
        OJson out;
        if (!mesh.name.empty())
            out["name"] = mesh.name;
        out["primitives"] = std::move(primitives);
        state->meshes.push_back(std::move(out));
        return int32_t(state->meshes.size() - 1);
    }

    int32_t GLTFWriter::addNode(const WriterNode &node)
    {
        OJson out = OJson::object();
        if (!node.name.empty())
            out["name"] = node.name;
        if (node.mesh >= 0)
        {
            checkIndex(node.mesh, state->meshes.size(), "mesh");
            out["mesh"] = node.mesh;
        }
        if (!node.children.empty())
        {
            // Children are added before their parent, so the hierarchy is a tree by construction.
            for (int32_t child : node.children)
                checkIndex(child, state->nodes.size(), "child node");
            out["children"] = node.children;
        }
        if (node.translation)
            out["translation"] = {finite((*node.translation)[0]), finite((*node.translation)[1]),
                                  finite((*node.translation)[2])};
        if (node.rotation)
            out["rotation"] = {finite((*node.rotation)[0]), finite((*node.rotation)[1]),
                               finite((*node.rotation)[2]), finite((*node.rotation)[3])};
        if (node.scale)
            out["scale"] = {finite((*node.scale)[0]), finite((*node.scale)[1]), finite((*node.scale)[2])};
        if (!node.extrasJson.empty())
        {
            OJson extras = OJson::parse(node.extrasJson, nullptr, false);
            if (extras.is_discarded() || !extras.is_object())
                throw std::invalid_argument("GLTFWriter: node extras must be a JSON object");
            out["extras"] = std::move(extras);
        }
        state->nodes.push_back(std::move(out));
        return int32_t(state->nodes.size() - 1);
    }

    int32_t GLTFWriter::addScene(const std::vector<int32_t> &nodes, const std::string &name, bool makeDefault)
    {
        for (int32_t node : nodes)
            checkIndex(node, state->nodes.size(), "node");
        OJson scene = OJson::object();
        if (!name.empty())
            scene["name"] = name;
        scene["nodes"] = nodes;
        state->scenes.push_back(std::move(scene));
        const int32_t index = int32_t(state->scenes.size() - 1);
        if (makeDefault || state->defaultScene < 0)
            state->defaultScene = index;
        return index;
    }

    void GLTFWriter::useExtension(const std::string &name, bool required)
    {
        auto add = [&](std::vector<std::string> &list) {
            if (std::find(list.begin(), list.end(), name) == list.end())
                list.push_back(name);
        };
        add(state->extensionsUsed);
        if (required)
            add(state->extensionsRequired);
    }

    std::string GLTFWriter::toJSON() const
    {
        OJson asset = {{"version", "2.0"}};
        if (!state->generator.empty())
            asset["generator"] = state->generator;
        OJson root = {{"asset", std::move(asset)}};
        if (!state->extensionsUsed.empty())
            root["extensionsUsed"] = state->extensionsUsed;
        if (!state->extensionsRequired.empty())
            root["extensionsRequired"] = state->extensionsRequired;
        if (state->defaultScene >= 0)
            root["scene"] = state->defaultScene;
        auto put = [&](const char *key, const OJson &list) {
            if (!list.empty())
                root[key] = list;
        };
        put("scenes", state->scenes);
        put("nodes", state->nodes);
        put("meshes", state->meshes);
        put("materials", state->materials);
        put("textures", state->textures);
        put("images", state->images);
        put("samplers", state->samplers);
        put("accessors", state->accessors);
        put("bufferViews", state->bufferViews);
        if (!state->bin.empty())
            root["buffers"] = OJson::array({{{"byteLength", (state->bin.size() + 3) & ~size_t(3)}}});
        return root.dump();
    }

    std::vector<uint8_t> GLTFWriter::toGLB() const
    {
        std::string json = toJSON();
        json.resize((json.size() + 3) & ~size_t(3), ' ');
        const size_t binLength = (state->bin.size() + 3) & ~size_t(3);
        const uint64_t total = 12 + 8 + json.size() + (binLength ? 8 + binLength : 0);
        if (total > std::numeric_limits<uint32_t>::max())
            throw std::length_error("GLTFWriter: the asset exceeds the 4 GB GLB limit");

        std::vector<uint8_t> out;
        out.reserve(size_t(total));
        put32(out, 0x46546C67u);  // "glTF"
        put32(out, 2);
        put32(out, uint32_t(total));
        put32(out, uint32_t(json.size()));
        put32(out, 0x4E4F534Au);  // "JSON"
        out.insert(out.end(), json.begin(), json.end());
        if (binLength)
        {
            put32(out, uint32_t(binLength));
            put32(out, 0x004E4942u);  // "BIN\0"
            out.insert(out.end(), state->bin.begin(), state->bin.end());
            out.resize(out.size() + (binLength - state->bin.size()), 0);
        }
        return out;
    }

}
