/**
 * Negative / malformed-input tests.
 *
 * Every test exercises a code path that MUST throw or return a safe sentinel
 * (nullptr) when given invalid data. No external files are needed.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::shared_ptr<GLTF> parse(const std::string &json)
{
    return GLTF::loadGLTF(json);
}

// Minimal valid glTF with a single scene→node→mesh→primitive chain so that
// getRuntimeInfo(0) will actually traverse the graph.
static std::string wrapScene(const std::string &extra)
{
    return R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes":  [{"mesh": 0}],
  "meshes": [{"primitives": [)" + extra + R"(]}]
})";
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Missing / malformed asset fields
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, MissingAssetField)
{
    EXPECT_THROW(parse(R"({"meshes": []})"), std::invalid_argument);
}

TEST(ValidationTest, MissingVersionField)
{
    EXPECT_THROW(parse(R"({"asset": {}})"), std::invalid_argument);
}

TEST(ValidationTest, WrongMajorVersion)
{
    EXPECT_THROW(parse(R"({"asset": {"version": "1.0"}})"), std::invalid_argument);
}

TEST(ValidationTest, MalformedVersionString)
{
    EXPECT_THROW(parse(R"({"asset": {"version": "noversion"}})"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Unsupported required extension
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, UnsupportedRequiredExtension)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "extensionsRequired": ["FAKE_unsupported_xyz"]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Buffer / payload errors
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, BufferMissingByteLength)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "buffers": [{"uri": "data:application/octet-stream;base64,AAAA"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, BufferViewMissingBuffer)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "bufferViews": [{"byteLength": 4}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, BufferViewMissingByteLength)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "bufferViews": [{"buffer": 0}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, AccessorMissingComponentType)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "accessors": [{"count": 1, "type": "SCALAR"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, AccessorMissingCount)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "accessors": [{"componentType": 5126, "type": "SCALAR"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, MeshMissingPrimitives)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "meshes": [{"name": "m"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, CameraMissingType)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "cameras": [{"name": "cam"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, PerspectiveCameraMissingPerspectiveObject)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "cameras": [{"type": "perspective"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, PerspectiveCameraMissingYfov)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "cameras": [{"type": "perspective", "perspective": {"znear": 0.01}}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, OrthographicCameraMissingOrthographicObject)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "cameras": [{"type": "orthographic"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, OrthographicCameraMissingXmag)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "cameras": [{"type": "orthographic", "orthographic": {"ymag": 1.0, "znear": 0.1, "zfar": 100.0}}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, MeshoptCompressionMissingByteLength)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "bufferViews": [{
            "buffer": 0, "byteLength": 4,
            "extensions": {
                "EXT_meshopt_compression": {"byteStride": 4, "count": 1}
            }
        }],
        "buffers": [{"byteLength": 4}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, BufferPayloadLengthMismatch)
{
    // byteLength says 999 but base64 payload decodes to 3 bytes
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 999, "uri": "data:application/octet-stream;base64,AAAA"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, BufferByteLengthExceedsLimit)
{
    // 3 GB > GLTF_MAX_BUFFER_BYTE_LENGTH (2 GB)
    uint64_t tooBig = 3ULL * 1024 * 1024 * 1024;
    EXPECT_THROW(parse(
        R"({"asset": {"version": "2.0"}, "buffers": [{"byteLength": )" +
        std::to_string(tooBig) + "}]}"),
        std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Array size limits
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, TooManyNodes)
{
    std::string json = R"({"asset": {"version": "2.0"}, "nodes": [)";
    for (int i = 0; i < 65537; ++i)
    {
        if (i > 0) json += ',';
        json += R"({"name":"n"})";
    }
    json += "]}";
    EXPECT_THROW(parse(json), std::invalid_argument);
}

TEST(ValidationTest, TooManyMeshes)
{
    std::string json = R"({"asset": {"version": "2.0"}, "meshes": [)";
    for (int i = 0; i < 65537; ++i)
    {
        if (i > 0) json += ',';
        json += R"({"primitives":[]})";
    }
    json += "]}";
    EXPECT_THROW(parse(json), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Invalid accessor type string
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, InvalidAccessorType)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 1, "type": "INVALID"}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Out-of-range index checks (detected at getRuntimeInfo time)
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, SceneNodeIndexOutOfRange)
{
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [99]}],
        "nodes":  []
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, NodeMeshIndexOutOfRange)
{
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes":  [{"mesh": 99}],
        "meshes": []
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, PrimitiveMaterialIndexOutOfRange)
{
    auto asset = parse(wrapScene(R"({"attributes": {}, "material": 99})"));
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, PrimitiveIndicesAccessorOutOfRange)
{
    auto asset = parse(wrapScene(R"({"attributes": {}, "indices": 99})"));
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, AttributeAccessorIndexOutOfRange)
{
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes":  [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 99}}]}],
        "accessors": []
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, AccessorBufferViewIndexOutOfRange)
{
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes":  [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
        "accessors":   [{"bufferView": 99, "componentType": 5126, "count": 1, "type": "VEC3"}],
        "bufferViews": []
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, MaterialTextureIndexOutOfRange)
{
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes":  [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {}, "material": 0}]}],
        "materials": [{"emissiveTexture": {"index": 99}}],
        "textures": []
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, NodeChildIndexOutOfRange)
{
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes":  [{"children": [99]}]
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Cycle detection in node hierarchy
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, DirectNodeCycle)
{
    // node 0 → child 1 → child 0  (cycle back to 0)
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"children": [1]},
            {"children": [0]}
        ]
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, SelfReferenceNodeCycle)
{
    // node 0 lists itself as a child
    auto asset = parse(R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"children": [0]}]
    })");
    EXPECT_THROW(asset->getRuntimeInfo(0), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. getRuntimeInfo safe sentinel for out-of-range scene index
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, GetRuntimeInfoOutOfRangeSceneIndex)
{
    auto asset = parse(R"({"asset": {"version": "2.0"}, "scenes": []})");
    EXPECT_EQ(asset->getRuntimeInfo(0), nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. Malformed JSON
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, MalformedJSON)
{
    EXPECT_THROW(parse("{not valid json"), std::exception);
}

TEST(ValidationTest, EmptyString)
{
    EXPECT_THROW(parse(""), std::exception);
}

// ─────────────────────────────────────────────────────────────────────────────
// 10. GLB binary format errors
// ─────────────────────────────────────────────────────────────────────────────

// Write a uint32_t in little-endian into buf at offset.
static void writeU32(std::vector<uint8_t> &buf, size_t offset, uint32_t v)
{
    buf[offset]     =  v        & 0xFF;
    buf[offset + 1] = (v >>  8) & 0xFF;
    buf[offset + 2] = (v >> 16) & 0xFF;
    buf[offset + 3] = (v >> 24) & 0xFF;
}

static std::vector<uint8_t> makeGLBHeader(uint32_t magic, uint32_t version, uint32_t length)
{
    std::vector<uint8_t> h(12, 0);
    writeU32(h, 0, magic);
    writeU32(h, 4, version);
    writeU32(h, 8, length);
    return h;
}

static constexpr uint32_t GLB_MAGIC   = 0x46546C67; // "glTF"
static constexpr uint32_t CHUNK_JSON  = 0x4E4F534A;
static constexpr uint32_t CHUNK_BIN   = 0x004E4942;

TEST(ValidationTest, GLBTooSmall)
{
    uint8_t data[] = {0x67, 0x6C, 0x54, 0x46}; // only 4 bytes
    EXPECT_THROW(GLTF::loadGLB(data, 4), std::invalid_argument);
}

TEST(ValidationTest, GLBInvalidMagic)
{
    auto h = makeGLBHeader(0xDEADBEEF, 2, 12);
    EXPECT_THROW(GLTF::loadGLB(h.data(), h.size()), std::invalid_argument);
}

TEST(ValidationTest, GLBWrongVersion)
{
    auto h = makeGLBHeader(GLB_MAGIC, 1, 12);
    EXPECT_THROW(GLTF::loadGLB(h.data(), h.size()), std::invalid_argument);
}

TEST(ValidationTest, GLBSizeMismatch)
{
    // Header says length=12 but we pass 20 bytes
    auto h = makeGLBHeader(GLB_MAGIC, 2, 12);
    h.resize(20, 0);
    EXPECT_THROW(GLTF::loadGLB(h.data(), h.size()), std::invalid_argument);
}

TEST(ValidationTest, GLBFirstChunkNotJSON)
{
    // Header (12) + chunk header (8) + 4 bytes padding = 24 total
    std::vector<uint8_t> data(24, 0);
    writeU32(data,  0, GLB_MAGIC);
    writeU32(data,  4, 2);          // version
    writeU32(data,  8, 24);         // total length
    writeU32(data, 12, 4);          // chunk0 length
    writeU32(data, 16, CHUNK_BIN);  // chunk0 type = BIN, not JSON
    EXPECT_THROW(GLTF::loadGLB(data.data(), data.size()), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Cross-reference validation
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, SparseAccessorCountExceedsBase)
{
    EXPECT_THROW(parse(R"({
      "asset": {"version": "2.0"},
      "bufferViews": [{"buffer": 0, "byteLength": 4}],
      "accessors": [{
        "bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR",
        "sparse": {
          "count": 5,
          "indices": {"bufferView": 0, "byteOffset": 0, "componentType": 5123},
          "values":  {"bufferView": 0, "byteOffset": 0}
        }
      }],
      "buffers": [{"byteLength": 4}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, SkinJointIndexOutOfRange)
{
    EXPECT_THROW(parse(R"({
      "asset": {"version": "2.0"},
      "nodes": [{}],
      "skins": [{"joints": [99]}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, AnimationChannelSamplerIndexOutOfRange)
{
    EXPECT_THROW(parse(R"({
      "asset": {"version": "2.0"},
      "nodes": [{}],
      "animations": [{
        "samplers": [{"input": 0, "output": 0}],
        "channels": [{"sampler": 5, "target": {"node": 0, "path": "translation"}}]
      }]
    })"), std::invalid_argument);
}

TEST(ValidationTest, KHRLightMissingType)
{
    EXPECT_THROW(parse(R"({
      "asset": {"version": "2.0"},
      "extensions": {
        "KHR_lights_punctual": {"lights": [{"intensity": 1.0}]}
      },
      "extensionsUsed": ["KHR_lights_punctual"]
    })"), std::invalid_argument);
}

TEST(ValidationTest, NodeLightIndexOutOfRange)
{
    // One light defined, node references index 5
    auto gltf = parse(R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"extensions": {"KHR_lights_punctual": {"light": 5}}}],
      "extensions": {
        "KHR_lights_punctual": {"lights": [{"type": "directional"}]}
      },
      "extensionsUsed": ["KHR_lights_punctual"]
    })");
    EXPECT_THROW(gltf->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, MaterialVariantMappingMaterialIndexOutOfRange)
{
    auto gltf = parse(R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}],
      "meshes": [{"primitives": [{
        "attributes": {},
        "extensions": {
          "KHR_materials_variants": {
            "mappings": [{"material": 99, "variants": [0]}]
          }
        }
      }]}],
      "extensions": {
        "KHR_materials_variants": {"variants": [{"name": "v0"}]}
      },
      "extensionsUsed": ["KHR_materials_variants"]
    })");
    EXPECT_THROW(gltf->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, MaterialVariantMappingVariantIndexOutOfRange)
{
    auto gltf = parse(R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}],
      "materials": [{"name": "mat0"}],
      "meshes": [{"primitives": [{
        "attributes": {},
        "extensions": {
          "KHR_materials_variants": {
            "mappings": [{"material": 0, "variants": [99]}]
          }
        }
      }]}],
      "extensions": {
        "KHR_materials_variants": {"variants": [{"name": "v0"}]}
      },
      "extensionsUsed": ["KHR_materials_variants"]
    })");
    EXPECT_THROW(gltf->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, TextureInfoMissingIndex)
{
    // Material with a normalTexture object that has no "index" field
    EXPECT_THROW(parse(R"({
      "asset": {"version": "2.0"},
      "materials": [{"normalTexture": {"scale": 1.0}}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Camera downcast
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, PerspectiveCameraDowncast)
{
    auto gltf = parse(R"({
      "asset": {"version": "2.0"},
      "cameras": [{
        "type": "perspective",
        "perspective": {"yfov": 0.785, "znear": 0.01, "zfar": 1000.0, "aspectRatio": 1.777}
      }]
    })");
    ASSERT_EQ(gltf->cameras->size(), 1u);
    auto cam = std::dynamic_pointer_cast<PerspectiveCamera>(gltf->cameras->at(0));
    ASSERT_NE(cam, nullptr);
    EXPECT_NEAR(cam->yFov, 0.785, 1e-6);
    EXPECT_NEAR(cam->zNear, 0.01, 1e-6);
    ASSERT_TRUE(cam->zFar.has_value());
    EXPECT_NEAR(cam->zFar.value(), 1000.0, 1e-6);
    ASSERT_TRUE(cam->aspectRatio.has_value());
    EXPECT_NEAR(cam->aspectRatio.value(), 1.777, 1e-6);
}

TEST(ValidationTest, OrthographicCameraDowncast)
{
    auto gltf = parse(R"({
      "asset": {"version": "2.0"},
      "cameras": [{
        "type": "orthographic",
        "orthographic": {"xmag": 2.0, "ymag": 1.5, "znear": 0.1, "zfar": 100.0}
      }]
    })");
    ASSERT_EQ(gltf->cameras->size(), 1u);
    auto cam = std::dynamic_pointer_cast<OrthographicCamera>(gltf->cameras->at(0));
    ASSERT_NE(cam, nullptr);
    EXPECT_NEAR(cam->xMag, 2.0, 1e-6);
    EXPECT_NEAR(cam->yMag, 1.5, 1e-6);
    EXPECT_NEAR(cam->zNear, 0.1, 1e-6);
    EXPECT_NEAR(cam->zFar, 100.0, 1e-6);
}
