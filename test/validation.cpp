/**
 * Negative / malformed-input tests.
 *
 * Every test exercises a code path that MUST throw or return a safe sentinel
 * (nullptr) when given invalid data. No external files are needed.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <future>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::shared_ptr<GLTF> parse(const std::string &json)
{
    // Self-contained test glTF (no external resources)
    return GLTF::loadGLTF(json, [](const std::string &) {
        return std::async(std::launch::deferred, []() {
            return std::vector<uint8_t>{};
        });
    });
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

TEST(ValidationTest, ValidVersion20)
{
    EXPECT_NO_THROW(parse(R"({"asset": {"version": "2.0"}})"));
}

TEST(ValidationTest, ValidVersion21)
{
    EXPECT_NO_THROW(parse(R"({"asset": {"version": "2.1"}})"));
}

TEST(ValidationTest, FutureMinorVersion21)
{
    // Any 2.x minor version is accepted; forward compatibility with future 2.x specs.
    EXPECT_NO_THROW(parse(R"({"asset": {"version": "2.99"}})"));
}

TEST(ValidationTest, Version30Rejected)
{
    EXPECT_THROW(parse(R"({"asset": {"version": "3.0"}})"), std::invalid_argument);
}

TEST(ValidationTest, NegativeMinorVersionRejected)
{
    EXPECT_THROW(parse(R"({"asset": {"version": "2.-1"}})"), std::invalid_argument);
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
    // glTF 2.1 raised the buffer limit to 8 EiB; use one byte more to trip the check.
    uint64_t tooBig = 0x7FFFFFFFFFFFFFFFULL + 1;
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

TEST(ValidationTest, MeshPrimitiveRestartGroupIndexOutOfRange)
{
    // The restart group references primitive index 2, but the mesh has only two primitives.
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "extensionsUsed": ["EXT_mesh_primitive_restart"],
        "meshes": [{
          "primitives": [
            {
              "attributes": {},
              "extensions": {
                "EXT_mesh_primitive_restart": {
                  "primitiveGroups": [{"primitives": [2], "indices": 0}]
                }
              }
            },
            {"attributes": {}}
          ]
        }]
    })"), std::invalid_argument);
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

TEST(ValidationTest, NodeIESLightIndexOutOfRange)
{
    auto gltf = parse(R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"extensions": {"EXT_lights_ies": {"light": 5}}}],
      "extensions": {
        "EXT_lights_ies": {"lights": [{"name": "ProfileA", "uri": "a.ies"}]}
      },
      "extensionsUsed": ["EXT_lights_ies"]
    })");
    EXPECT_THROW(gltf->getRuntimeInfo(0), std::invalid_argument);
}

TEST(ValidationTest, SceneImageBasedLightIndexOutOfRange)
{
    auto gltf = parse(R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{
        "nodes": [],
        "extensions": {"EXT_lights_image_based": {"light": 5}}
      }],
      "extensions": {
        "EXT_lights_image_based": {
          "lights": [{"name": "IBL", "rotation": [0, 0, 0, 1]}]
        }
      },
      "extensionsUsed": ["EXT_lights_image_based"]
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

// ─────────────────────────────────────────────────────────────────────────────
// 10. glTF 2.1 new array references
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, ThumbnailIndexOutOfRange)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.1", "thumbnail": 0}
    })"), std::invalid_argument);
}

TEST(ValidationTest, ExternalAssetFileIndexOutOfRange)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.1"},
        "externalAssets": [{"file": 0}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, NodeExternalAssetIndexOutOfRange)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"externalAsset": 0}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, BoundingVolumeShapeIndexOutOfRange)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"boundingVolume": {"shape": 0}}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, FileBufferViewIndexOutOfRange)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.1"},
        "files": [{"mimeType": "model/gltf-binary", "bufferView": 0}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, Valid21ReferencesAccepted)
{
    auto gltf = parse(R"({
        "asset": {"version": "2.1", "thumbnail": 0},
        "images": [{"mimeType": "image/png", "uri": "thumb.png"}],
        "files": [{"mimeType": "model/gltf-binary", "uri": "sub.glb"}],
        "externalAssets": [{"name": "sub", "file": 0}],
        "shapes": [{"type": "box", "box": {"size": [1,1,1]}}],
        "nodes": [
            {"externalAsset": 0},
            {"boundingVolume": {"shape": 0}}
        ]
    })");
    ASSERT_EQ(gltf->thumbnail, 0);
    ASSERT_EQ(gltf->externalAssets->size(), 1u);
    ASSERT_EQ(gltf->shapes->size(), 1u);
    EXPECT_EQ((*gltf->nodes)[0].externalAsset, 0);
    ASSERT_NE((*gltf->nodes)[1].boundingVolume, nullptr);
    EXPECT_EQ((*gltf->nodes)[1].boundingVolume->shape, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 11. RuntimeInfo image-index bounds checking
// ─────────────────────────────────────────────────────────────────────────────

static std::string textureRuntimeInfoScene(const std::string &textureDef)
{
    return R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}],
  "accessors": [{"componentType": 5126, "count": 1, "type": "VEC3"}],
  "bufferViews": [{"buffer": 0, "byteLength": 12}],
  "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAA", "byteLength": 12}],
  "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
  "textures": [)" + textureDef + R"(]
})";
}

TEST(ValidationTest, RuntimeInfoTextureSourceOutOfRange)
{
    EXPECT_THROW(
        parse(textureRuntimeInfoScene(R"({"source": 5})"))->getRuntimeInfo(0),
        std::invalid_argument);
}

TEST(ValidationTest, RuntimeInfoWebpSourceOutOfRange)
{
    EXPECT_THROW(
        parse(textureRuntimeInfoScene(R"({
            "extensions": {"EXT_texture_webp": {"source": 5}}
        })"))->getRuntimeInfo(0),
        std::invalid_argument);
}

TEST(ValidationTest, RuntimeInfoBasisuSourceOutOfRange)
{
    EXPECT_THROW(
        parse(textureRuntimeInfoScene(R"({
            "extensions": {"KHR_texture_basisu": {"source": 5}}
        })"))->getRuntimeInfo(0),
        std::invalid_argument);
}

TEST(ValidationTest, RuntimeInfoAstcSourceOutOfRange)
{
    EXPECT_THROW(
        parse(textureRuntimeInfoScene(R"({
            "extensions": {"EXT_texture_astc": {"source": 5}}
        })"))->getRuntimeInfo(0),
        std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 12. Animation required-field validation
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, AnimationSamplerMissingInput)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "animations": [{
            "samplers": [{"output": 0}],
            "channels": []
        }]
    })"), std::invalid_argument);
}

TEST(ValidationTest, AnimationSamplerMissingOutput)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "animations": [{
            "samplers": [{"input": 0}],
            "channels": []
        }]
    })"), std::invalid_argument);
}

TEST(ValidationTest, AnimationChannelMissingSampler)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "animations": [{
            "samplers": [{"input": 0, "output": 1}],
            "channels": [{"target": {"node": 0, "path": "translation"}}]
        }]
    })"), std::invalid_argument);
}

TEST(ValidationTest, AnimationChannelTargetMissingPath)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "animations": [{
            "samplers": [{"input": 0, "output": 1}],
            "channels": [{"sampler": 0, "target": {"node": 0}}]
        }]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 13. Skin required-field validation
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, SkinMissingJoints)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "skins": [{"name": "NoJoints"}]
    })"), std::invalid_argument);
}

TEST(ValidationTest, SkinEmptyJoints)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "skins": [{"name": "EmptyJoints", "joints": []}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 14. Sparse accessor indices componentType validation
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, SparseAccessorInvalidComponentType)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "accessors": [{
            "componentType": 5126,
            "count": 3,
            "type": "SCALAR",
            "sparse": {
                "count": 1,
                "indices": {"bufferView": 0, "componentType": 5126},
                "values": {"bufferView": 0}
            }
        }],
        "bufferViews": [{"buffer": 0, "byteLength": 12}],
        "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAA", "byteLength": 12}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 15. KHR_lights_punctual numeric constraint validation
// ─────────────────────────────────────────────────────────────────────────────

TEST(ValidationTest, LightRangeNonPositive)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "extensionsUsed": ["KHR_lights_punctual"],
        "extensions": {
            "KHR_lights_punctual": {
                "lights": [{"type": "point", "range": 0}]
            }
        }
    })"), std::invalid_argument);
}

TEST(ValidationTest, LightSpotInvalidConeAngles)
{
    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "extensionsUsed": ["KHR_lights_punctual"],
        "extensions": {
            "KHR_lights_punctual": {
                "lights": [{
                    "type": "spot",
                    "spot": {"innerConeAngle": -0.1, "outerConeAngle": 0.5}
                }]
            }
        }
    })"), std::invalid_argument);

    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "extensionsUsed": ["KHR_lights_punctual"],
        "extensions": {
            "KHR_lights_punctual": {
                "lights": [{
                    "type": "spot",
                    "spot": {"innerConeAngle": 0.5, "outerConeAngle": 0.5}
                }]
            }
        }
    })"), std::invalid_argument);

    EXPECT_THROW(parse(R"({
        "asset": {"version": "2.0"},
        "extensionsUsed": ["KHR_lights_punctual"],
        "extensions": {
            "KHR_lights_punctual": {
                "lights": [{
                    "type": "spot",
                    "spot": {"innerConeAngle": 0.0, "outerConeAngle": 2.0}
                }]
            }
        }
    })"), std::invalid_argument);
}


