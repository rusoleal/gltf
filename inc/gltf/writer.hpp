#pragma once

#include <gltf/accessor.hpp>
#include <gltf/buffer_view.hpp>
#include <gltf/material.hpp>
#include <gltf/mesh.hpp>
#include <gltf/sampler.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace systems::leal::gltf
{

    /**
     * A PBR metallic-roughness material to write (core glTF 2.0, plus
     * KHR_materials_unlit). Texture fields are texture indices from
     * GLTFWriter::addTexture(), -1 for none; they use texture coordinate set 0.
     */
    struct WriterMaterial
    {
        std::string name;
        std::array<double, 4> baseColorFactor = {1.0, 1.0, 1.0, 1.0};
        double metallicFactor = 1.0;
        double roughnessFactor = 1.0;
        int32_t baseColorTexture = -1;
        int32_t metallicRoughnessTexture = -1;
        int32_t normalTexture = -1;
        double normalScale = 1.0;
        int32_t occlusionTexture = -1;
        int32_t emissiveTexture = -1;
        std::array<double, 3> emissiveFactor = {0.0, 0.0, 0.0};
        AlphaMode alphaMode = AlphaMode::opaque;
        double alphaCutoff = 0.5;
        bool doubleSided = false;
        /// KHR_materials_unlit (added to extensionsUsed).
        bool unlit = false;
    };

    /**
     * A primitive to write: attribute semantic ("POSITION", "NORMAL",
     * "COLOR_0", "TEXCOORD_0"...) to accessor index.
     */
    struct WriterPrimitive
    {
        std::map<std::string, int32_t> attributes;
        int32_t indices = -1;
        int32_t material = -1;
        PrimitiveMode mode = PrimitiveMode::pmTriangles;
    };

    struct WriterMesh
    {
        std::string name;
        std::vector<WriterPrimitive> primitives;
    };

    struct WriterNode
    {
        std::string name;
        int32_t mesh = -1;
        std::vector<int32_t> children;
        std::optional<std::array<double, 3>> translation;
        std::optional<std::array<double, 4>> rotation;  ///< Quaternion x, y, z, w.
        std::optional<std::array<double, 3>> scale;
        /// A JSON object written as the node's "extras" (empty: none). Throws
        /// std::invalid_argument from addNode() if it isn't a JSON object.
        std::string extrasJson;
    };

    /**
     * Builds a glTF 2.0 asset and writes it as binary glTF (GLB): one buffer
     * (the GLB's BIN chunk) holding every buffer view, 4-byte aligned.
     *
     * Indices returned by the add* methods are the glTF indices of the
     * objects, for referencing them from later objects. Everything is in glTF
     * conventions: Y up, meters, right-handed, counter-clockwise front faces.
     *
     * @code
     * GLTFWriter writer("my app 1.0");
     * WriterPrimitive primitive;
     * primitive.attributes["POSITION"] = writer.addPositions(xyz, vertexCount);
     * primitive.attributes["COLOR_0"] = writer.addColors(rgb, vertexCount, AccessorType::acVec3);
     * primitive.indices = writer.addIndices(indices, indexCount);
     * WriterNode node;
     * node.mesh = writer.addMesh({"scan", {primitive}});
     * writer.addScene({writer.addNode(node)});
     * std::vector<uint8_t> glb = writer.toGLB();
     * @endcode
     */
    class GLTFWriter
    {
    public:
        /// `generator` is written as asset.generator.
        explicit GLTFWriter(std::string generator = {});
        ~GLTFWriter();
        GLTFWriter(const GLTFWriter &) = delete;
        GLTFWriter &operator=(const GLTFWriter &) = delete;

        /// Copies `byteLength` bytes into the binary buffer (4-byte aligned)
        /// as a new buffer view.
        int32_t addBufferView(const void *data, uint64_t byteLength,
                              BufferViewTarget target = BufferViewTarget::undefined, uint32_t byteStride = 0);

        /// An accessor over `bufferView`. `min` / `max` are written when not
        /// empty (glTF requires them for POSITION).
        int32_t addAccessor(int32_t bufferView, ComponentType componentType, AccessorType type, uint64_t count,
                            bool normalized = false, uint64_t byteOffset = 0, const std::vector<double> &min = {},
                            const std::vector<double> &max = {});

        /// Float vertex attribute (VEC2 / VEC3 / VEC4 / SCALAR), `count`
        /// elements; with `bounds`, its min and max are written.
        int32_t addFloatAttribute(const float *data, uint64_t count, AccessorType type, bool bounds = false);

        /// POSITION: `count` xyz triples, with the min / max glTF requires.
        int32_t addPositions(const float *xyz, uint64_t count);

        /// COLOR_n as normalized unsigned bytes: `type` acVec3 (RGB) or acVec4
        /// (RGBA). Linear values, as glTF defines vertex colors.
        int32_t addColors(const uint8_t *data, uint64_t count, AccessorType type);

        /// Triangle (or line, point) indices: stored as unsigned short when
        /// every index fits, else unsigned int.
        int32_t addIndices(const uint32_t *indices, uint64_t count);

        /// An encoded image (PNG, JPEG, KTX2...) stored in the binary buffer.
        int32_t addImage(const std::vector<uint8_t> &encoded, const std::string &mimeType,
                         const std::string &name = {});

        int32_t addSampler(FilterMode magFilter = FilterMode::fmLinear,
                           FilterMode minFilter = FilterMode::fmLinearMipmapLinear,
                           WrapMode wrapS = WrapMode::repeat, WrapMode wrapT = WrapMode::repeat);

        /// A texture of `image`; `sampler` -1 for the default sampler.
        /// With `basisu`, the image is the KHR_texture_basisu source (KTX2)
        /// and the extension is added to extensionsUsed and extensionsRequired.
        int32_t addTexture(int32_t image, int32_t sampler = -1, bool basisu = false);

        int32_t addMaterial(const WriterMaterial &material);
        int32_t addMesh(const WriterMesh &mesh);
        int32_t addNode(const WriterNode &node);

        /// A scene with root `nodes`; the first scene added is the default
        /// (asset "scene"), unless `makeDefault` picks another.
        int32_t addScene(const std::vector<int32_t> &nodes, const std::string &name = {}, bool makeDefault = false);

        /// Adds an extension name to extensionsUsed (and extensionsRequired).
        void useExtension(const std::string &name, bool required = false);

        /// The asset as GLB 2.0 bytes.
        std::vector<uint8_t> toGLB() const;

        /// The asset's JSON (as stored in the GLB's JSON chunk).
        std::string toJSON() const;

    private:
        struct State;
        std::unique_ptr<State> state;
    };

}
