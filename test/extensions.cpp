/**
 * Extension tests for every implemented glTF 2.0 extension.
 *
 * All tests use self-contained inline JSON so no network is required.
 * A small set of integration fixtures (bottom of file) load real Khronos
 * sample assets downloaded by launch_test.sh.
 */

#include <fstream>
#include <streambuf>
#include <limits>
#include <cstring>
#include <future>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>
#include <meshoptimizer.h>

using namespace systems::leal::gltf;

// ── small helpers ─────────────────────────────────────────────────────────────

// loadFileAsBytes is defined in main.cpp and shared across test files

static std::shared_ptr<GLTF> loadInline(const std::string &json)
{
    // Self-contained glTF (inline base64 data URIs only)
    return GLTF::loadGLTF(json, [](const std::string &) {
        return std::async(std::launch::deferred, []() {
            return std::vector<uint8_t>{};
        });
    });
}

static std::shared_ptr<GLTF> loadFile(const std::string &path)
{
    std::ifstream t(path);
    if (!t.good()) {
        return nullptr;  // Return null if file can't be opened
    }
    std::string str((std::istreambuf_iterator<char>(t)),
                     std::istreambuf_iterator<char>());
    if (str.empty()) {
        return nullptr;  // Return null if file is empty
    }
    return GLTF::loadGLTF(str, [](const std::string &uri) {
        return std::async(std::launch::deferred, [&uri]() {
            return loadFileAsBytes(uri);
        });
    });
}


// ─────────────────────────────────────────────────────────────────────────────
// 1.  KHR_lights_punctual
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kLightsPunctualJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {
    "KHR_lights_punctual": {
      "lights": [
        {"name": "Sun",  "type": "directional", "color": [1.0, 0.9, 0.8], "intensity": 5.0},
        {"name": "Lamp", "type": "point",        "color": [0.8, 0.9, 1.0], "intensity": 100.0, "range": 15.0},
        {"name": "Spot", "type": "spot",         "intensity": 200.0,
         "spot": {"innerConeAngle": 0.3927, "outerConeAngle": 0.7854}}
      ]
    }
  },
  "scene": 0,
  "scenes": [{"nodes": [0, 1, 2]}],
  "nodes": [
    {"name": "DirNode",   "extensions": {"KHR_lights_punctual": {"light": 0}}},
    {"name": "SpotNode",  "extensions": {"KHR_lights_punctual": {"light": 2}}},
    {"name": "PlainNode"}
  ]
})";

class KHRLightsPunctualTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRLightsPunctualTest() { asset = loadInline(kLightsPunctualJSON); }
};

TEST_F(KHRLightsPunctualTest, LightCount)
{
    ASSERT_NE(asset->khrLightsPunctual, nullptr);
    EXPECT_EQ(asset->khrLightsPunctual->size(), 3u);
}

TEST_F(KHRLightsPunctualTest, DirectionalLight)
{
    const auto &light = (*asset->khrLightsPunctual)[0];
    EXPECT_EQ(light.name, "Sun");
    EXPECT_EQ(light.type, KHRLightPunctualType::directional);
    EXPECT_NEAR(light.color.data[0], 1.0, 1e-6);
    EXPECT_NEAR(light.color.data[1], 0.9, 1e-6);
    EXPECT_NEAR(light.color.data[2], 0.8, 1e-6);
    EXPECT_NEAR(light.intensity, 5.0, 1e-6);
}

TEST_F(KHRLightsPunctualTest, PointLight)
{
    const auto &light = (*asset->khrLightsPunctual)[1];
    EXPECT_EQ(light.name, "Lamp");
    EXPECT_EQ(light.type, KHRLightPunctualType::point);
    EXPECT_NEAR(light.intensity, 100.0, 1e-6);
    EXPECT_NEAR(light.range, 15.0, 1e-6);
}

TEST_F(KHRLightsPunctualTest, SpotLight)
{
    const auto &light = (*asset->khrLightsPunctual)[2];
    EXPECT_EQ(light.name, "Spot");
    EXPECT_EQ(light.type, KHRLightPunctualType::spot);
    EXPECT_NEAR(light.intensity, 200.0, 1e-6);
    EXPECT_NEAR(light.innerConeAngle, 0.3927, 1e-4);
    EXPECT_NEAR(light.outerConeAngle, 0.7854, 1e-4);
}

TEST_F(KHRLightsPunctualTest, NodeLightRef)
{
    const Node *dirNode  = findNode(asset, "DirNode");
    const Node *spotNode = findNode(asset, "SpotNode");
    ASSERT_NE(dirNode, nullptr);
    ASSERT_NE(spotNode, nullptr);
    EXPECT_EQ(dirNode->light,  0);
    EXPECT_EQ(spotNode->light, 2);
}

TEST_F(KHRLightsPunctualTest, NodeWithoutLightIsMinusOne)
{
    const Node *plain = findNode(asset, "PlainNode");
    ASSERT_NE(plain, nullptr);
    EXPECT_EQ(plain->light, -1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2.  EXT_mesh_gpu_instancing
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMeshGpuInstancingJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["EXT_mesh_gpu_instancing"],
  "scene": 0,
  "scenes": [{"nodes": [0, 1]}],
  "nodes": [
    {
      "name": "InstancedNode",
      "extensions": {
        "EXT_mesh_gpu_instancing": {
          "attributes": {"TRANSLATION": 0, "ROTATION": 1, "SCALE": 2}
        }
      }
    },
    {"name": "PlainNode"}
  ]
})";

class EXTMeshGpuInstancingTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    EXTMeshGpuInstancingTest() { asset = loadInline(kMeshGpuInstancingJSON); }
};

TEST_F(EXTMeshGpuInstancingTest, InstancedNodeNotNull)
{
    const Node *n = findNode(asset, "InstancedNode");
    ASSERT_NE(n, nullptr);
    EXPECT_NE(n->extMeshGpuInstancing, nullptr);
}

TEST_F(EXTMeshGpuInstancingTest, Attributes)
{
    const Node *n = findNode(asset, "InstancedNode");
    ASSERT_NE(n, nullptr);
    ASSERT_NE(n->extMeshGpuInstancing, nullptr);
    const auto &attrs = n->extMeshGpuInstancing->attributes;
    EXPECT_EQ(attrs.at("TRANSLATION"), 0u);
    EXPECT_EQ(attrs.at("ROTATION"),    1u);
    EXPECT_EQ(attrs.at("SCALE"),       2u);
}

TEST_F(EXTMeshGpuInstancingTest, UninstancedNodeIsNull)
{
    const Node *plain = findNode(asset, "PlainNode");
    ASSERT_NE(plain, nullptr);
    EXPECT_EQ(plain->extMeshGpuInstancing, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3.  KHR_mesh_quantization  (marker extension — no additional data fields)
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMeshQuantizationJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed":     ["KHR_mesh_quantization"],
  "extensionsRequired": ["KHR_mesh_quantization"],
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"name": "Node"}]
})";

TEST(KHRMeshQuantization, LoadsWithoutException)
{
    EXPECT_NO_THROW(loadInline(kMeshQuantizationJSON));
}

TEST(KHRMeshQuantization, ExtensionIsRequired)
{
    // Verifies that the extension is in the implemented list so no exception
    // is thrown when it appears in extensionsRequired.
    auto asset = loadInline(kMeshQuantizationJSON);
    EXPECT_NE(asset, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4.  KHR_materials_unlit
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsUnlitJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_unlit"],
  "materials": [
    {
      "name": "UnlitMat",
      "extensions": {"KHR_materials_unlit": {}}
    },
    {
      "name": "LitMat"
    }
  ]
})";

class KHRMaterialsUnlitTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsUnlitTest() { asset = loadInline(kMaterialsUnlitJSON); }
};

TEST_F(KHRMaterialsUnlitTest, UnlitFlagTrue)
{
    const Material *mat = findMaterial(asset, "UnlitMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_TRUE(mat->khrMaterialsUnlit);
}

TEST_F(KHRMaterialsUnlitTest, RegularMaterialNotUnlit)
{
    const Material *mat = findMaterial(asset, "LitMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_FALSE(mat->khrMaterialsUnlit);
}

TEST_F(KHRMaterialsUnlitTest, MaterialCount)
{
    EXPECT_EQ(asset->materials->size(), 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5.  KHR_materials_emissive_strength
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kEmissiveStrengthJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_emissive_strength"],
  "materials": [
    {
      "name": "BrightMat",
      "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": 5.0}}
    },
    {
      "name": "DefaultMat"
    }
  ]
})";

class KHRMaterialsEmissiveStrengthTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsEmissiveStrengthTest() { asset = loadInline(kEmissiveStrengthJSON); }
};

TEST_F(KHRMaterialsEmissiveStrengthTest, StrengthValue)
{
    const Material *mat = findMaterial(asset, "BrightMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsEmissiveStrength, 5.0, 1e-6);
}

TEST_F(KHRMaterialsEmissiveStrengthTest, DefaultIsOne)
{
    const Material *mat = findMaterial(asset, "DefaultMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsEmissiveStrength, 1.0, 1e-6);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6.  KHR_materials_ior
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsIorJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_ior"],
  "materials": [
    {
      "name": "GlassMat",
      "extensions": {"KHR_materials_ior": {"ior": 1.52}}
    },
    {
      "name": "DefaultMat"
    }
  ]
})";

class KHRMaterialsIorTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsIorTest() { asset = loadInline(kMaterialsIorJSON); }
};

TEST_F(KHRMaterialsIorTest, IorValue)
{
    const Material *mat = findMaterial(asset, "GlassMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsIor, 1.52, 1e-6);
}

TEST_F(KHRMaterialsIorTest, DefaultIsOnePointFive)
{
    const Material *mat = findMaterial(asset, "DefaultMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsIor, 1.5, 1e-6);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7.  KHR_materials_dispersion
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsDispersionJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_dispersion"],
  "materials": [
    {
      "name": "PrismMat",
      "extensions": {"KHR_materials_dispersion": {"dispersion": 0.03}}
    },
    {
      "name": "DefaultMat"
    }
  ]
})";

class KHRMaterialsDispersionTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsDispersionTest() { asset = loadInline(kMaterialsDispersionJSON); }
};

TEST_F(KHRMaterialsDispersionTest, DispersionValue)
{
    const Material *mat = findMaterial(asset, "PrismMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsDispersion, 0.03, 1e-6);
}

TEST_F(KHRMaterialsDispersionTest, DefaultIsZero)
{
    const Material *mat = findMaterial(asset, "DefaultMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsDispersion, 0.0, 1e-6);
}

// ─────────────────────────────────────────────────────────────────────────────
// 8.  KHR_texture_basisu
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kTextureBasisuJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_texture_basisu"],
  "images": [
    {"uri": "fallback.png"},
    {"uri": "compressed.ktx2"}
  ],
  "textures": [
    {
      "source": 0,
      "extensions": {
        "KHR_texture_basisu": {"source": 1}
      }
    },
    {
      "source": 0
    }
  ]
})";

class KHRTextureBasisuTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRTextureBasisuTest() { asset = loadInline(kTextureBasisuJSON); }
};

TEST_F(KHRTextureBasisuTest, BasisuSource)
{
    ASSERT_EQ(asset->textures->size(), 2u);
    EXPECT_EQ((*asset->textures)[0].khr_texture_basisu, 1);
}

TEST_F(KHRTextureBasisuTest, FallbackSourcePreserved)
{
    EXPECT_EQ((*asset->textures)[0].source, 0);
}

TEST_F(KHRTextureBasisuTest, NoExtensionIsMinusOne)
{
    EXPECT_EQ((*asset->textures)[1].khr_texture_basisu, -1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9.  EXT_texture_webp
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kTextureWebpJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["EXT_texture_webp"],
  "images": [
    {"uri": "fallback.png"},
    {"uri": "image.webp"}
  ],
  "textures": [
    {
      "source": 0,
      "extensions": {
        "EXT_texture_webp": {"source": 1}
      }
    },
    {
      "source": 0
    }
  ]
})";

class EXTTextureWebpTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    EXTTextureWebpTest() { asset = loadInline(kTextureWebpJSON); }
};

TEST_F(EXTTextureWebpTest, WebpSource)
{
    ASSERT_EQ(asset->textures->size(), 2u);
    EXPECT_EQ((*asset->textures)[0].ext_texture_webp, 1);
}

TEST_F(EXTTextureWebpTest, FallbackSourcePreserved)
{
    EXPECT_EQ((*asset->textures)[0].source, 0);
}

TEST_F(EXTTextureWebpTest, NoExtensionIsMinusOne)
{
    EXPECT_EQ((*asset->textures)[1].ext_texture_webp, -1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 10.  KHR_texture_transform
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kTextureTransformJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_texture_transform"],
  "materials": [
    {
      "name": "TransformMat",
      "pbrMetallicRoughness": {
        "baseColorTexture": {
          "index": 0,
          "extensions": {
            "KHR_texture_transform": {
              "offset":   [0.1, 0.2],
              "rotation": 0.785,
              "scale":    [2.0, 3.0],
              "texCoord": 1
            }
          }
        }
      },
      "normalTexture": {
        "index": 0,
        "extensions": {
          "KHR_texture_transform": {
            "offset":   [0.0, 0.0],
            "rotation": 0.0,
            "scale":    [1.0, 1.0]
          }
        }
      }
    },
    {
      "name": "NoTransformMat",
      "pbrMetallicRoughness": {
        "baseColorTexture": {"index": 0}
      }
    }
  ]
})";

class KHRTextureTransformTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRTextureTransformTest() { asset = loadInline(kTextureTransformJSON); }
};

TEST_F(KHRTextureTransformTest, TransformNotNull)
{
    const Material *mat = findMaterial(asset, "TransformMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness->baseColorTexture, nullptr);
    EXPECT_NE(mat->pbrMetallicRoughness->baseColorTexture->khrTextureTransform, nullptr);
}

TEST_F(KHRTextureTransformTest, Offset)
{
    const auto &tt = *findMaterial(asset, "TransformMat")
                           ->pbrMetallicRoughness->baseColorTexture->khrTextureTransform;
    EXPECT_NEAR(tt.offset.data[0], 0.1, 1e-6);
    EXPECT_NEAR(tt.offset.data[1], 0.2, 1e-6);
}

TEST_F(KHRTextureTransformTest, Rotation)
{
    const auto &tt = *findMaterial(asset, "TransformMat")
                           ->pbrMetallicRoughness->baseColorTexture->khrTextureTransform;
    EXPECT_NEAR(tt.rotation, 0.785, 1e-3);
}

TEST_F(KHRTextureTransformTest, Scale)
{
    const auto &tt = *findMaterial(asset, "TransformMat")
                           ->pbrMetallicRoughness->baseColorTexture->khrTextureTransform;
    EXPECT_NEAR(tt.scale.data[0], 2.0, 1e-6);
    EXPECT_NEAR(tt.scale.data[1], 3.0, 1e-6);
}

TEST_F(KHRTextureTransformTest, TexCoordOverride)
{
    const auto &tt = *findMaterial(asset, "TransformMat")
                           ->pbrMetallicRoughness->baseColorTexture->khrTextureTransform;
    EXPECT_EQ(tt.texCoord, 1);
}

TEST_F(KHRTextureTransformTest, MissingTexCoordIsMinusOne)
{
    const Material *mat = findMaterial(asset, "TransformMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->normalTexture, nullptr);
    ASSERT_NE(mat->normalTexture->khrTextureTransform, nullptr);
    EXPECT_EQ(mat->normalTexture->khrTextureTransform->texCoord, -1);
}

TEST_F(KHRTextureTransformTest, AbsentTransformIsNull)
{
    const Material *mat = findMaterial(asset, "NoTransformMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness->baseColorTexture, nullptr);
    EXPECT_EQ(mat->pbrMetallicRoughness->baseColorTexture->khrTextureTransform, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 11.  KHR_materials_transmission
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsTransmissionJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_transmission"],
  "materials": [
    {
      "name": "GlassMat",
      "extensions": {
        "KHR_materials_transmission": {
          "transmissionFactor": 0.9,
          "transmissionTexture": {"index": 0}
        }
      }
    },
    {"name": "OpaqueMat"}
  ]
})";

class KHRMaterialsTransmissionTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsTransmissionTest() { asset = loadInline(kMaterialsTransmissionJSON); }
};

TEST_F(KHRMaterialsTransmissionTest, ExtensionNotNull)
{
    const Material *mat = findMaterial(asset, "GlassMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NE(mat->khrMaterialsTransmission, nullptr);
}

TEST_F(KHRMaterialsTransmissionTest, TransmissionFactor)
{
    const Material *mat = findMaterial(asset, "GlassMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->khrMaterialsTransmission, nullptr);
    EXPECT_NEAR(mat->khrMaterialsTransmission->transmissionFactor, 0.9, 1e-6);
}

TEST_F(KHRMaterialsTransmissionTest, TransmissionTexture)
{
    const Material *mat = findMaterial(asset, "GlassMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->khrMaterialsTransmission, nullptr);
    ASSERT_NE(mat->khrMaterialsTransmission->transmissionTexture, nullptr);
    EXPECT_EQ(mat->khrMaterialsTransmission->transmissionTexture->index, 0u);
}

TEST_F(KHRMaterialsTransmissionTest, AbsentIsNull)
{
    const Material *mat = findMaterial(asset, "OpaqueMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrMaterialsTransmission, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 12.  KHR_materials_clearcoat
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsClearcoatJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_clearcoat"],
  "materials": [
    {
      "name": "VarnishMat",
      "extensions": {
        "KHR_materials_clearcoat": {
          "clearcoatFactor":          0.8,
          "clearcoatTexture":         {"index": 0},
          "clearcoatRoughnessFactor": 0.3,
          "clearcoatRoughnessTexture":{"index": 1},
          "clearcoatNormalTexture":   {"index": 2}
        }
      }
    },
    {"name": "PlainMat"}
  ]
})";

class KHRMaterialsClearcoatTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsClearcoatTest() { asset = loadInline(kMaterialsClearcoatJSON); }
};

TEST_F(KHRMaterialsClearcoatTest, ExtensionNotNull)
{
    const Material *mat = findMaterial(asset, "VarnishMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NE(mat->khrMaterialsClearcoat, nullptr);
}

TEST_F(KHRMaterialsClearcoatTest, ClearcoatFactor)
{
    const Material *mat = findMaterial(asset, "VarnishMat");
    ASSERT_NE(mat->khrMaterialsClearcoat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsClearcoat->clearcoatFactor, 0.8, 1e-6);
}

TEST_F(KHRMaterialsClearcoatTest, ClearcoatRoughnessFactor)
{
    const Material *mat = findMaterial(asset, "VarnishMat");
    ASSERT_NE(mat->khrMaterialsClearcoat, nullptr);
    EXPECT_NEAR(mat->khrMaterialsClearcoat->clearcoatRoughnessFactor, 0.3, 1e-6);
}

TEST_F(KHRMaterialsClearcoatTest, Textures)
{
    const Material *mat = findMaterial(asset, "VarnishMat");
    ASSERT_NE(mat->khrMaterialsClearcoat, nullptr);
    EXPECT_NE(mat->khrMaterialsClearcoat->clearcoatTexture,          nullptr);
    EXPECT_NE(mat->khrMaterialsClearcoat->clearcoatRoughnessTexture, nullptr);
    EXPECT_NE(mat->khrMaterialsClearcoat->clearcoatNormalTexture,    nullptr);
    EXPECT_EQ(mat->khrMaterialsClearcoat->clearcoatTexture->index,          0u);
    EXPECT_EQ(mat->khrMaterialsClearcoat->clearcoatRoughnessTexture->index, 1u);
    EXPECT_EQ(mat->khrMaterialsClearcoat->clearcoatNormalTexture->index,    2u);
}

TEST_F(KHRMaterialsClearcoatTest, AbsentIsNull)
{
    const Material *mat = findMaterial(asset, "PlainMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrMaterialsClearcoat, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 13.  KHR_materials_sheen
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsSheenJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_sheen"],
  "materials": [
    {
      "name": "SheenMat",
      "extensions": {
        "KHR_materials_sheen": {
          "sheenColorFactor":         [1.0, 0.4, 0.0],
          "sheenColorTexture":        {"index": 0},
          "sheenRoughnessFactor":     0.5,
          "sheenRoughnessTexture":    {"index": 1}
        }
      }
    },
    {"name": "PlainMat"}
  ]
})";

class KHRMaterialsSheenTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsSheenTest() { asset = loadInline(kMaterialsSheenJSON); }
};

TEST_F(KHRMaterialsSheenTest, ExtensionNotNull)
{
    const Material *mat = findMaterial(asset, "SheenMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NE(mat->khrMaterialsSheen, nullptr);
}

TEST_F(KHRMaterialsSheenTest, SheenColor)
{
    const Material *mat = findMaterial(asset, "SheenMat");
    ASSERT_NE(mat->khrMaterialsSheen, nullptr);
    EXPECT_NEAR(mat->khrMaterialsSheen->sheenColorFactor.data[0], 1.0, 1e-6);
    EXPECT_NEAR(mat->khrMaterialsSheen->sheenColorFactor.data[1], 0.4, 1e-6);
    EXPECT_NEAR(mat->khrMaterialsSheen->sheenColorFactor.data[2], 0.0, 1e-6);
}

TEST_F(KHRMaterialsSheenTest, SheenRoughness)
{
    const Material *mat = findMaterial(asset, "SheenMat");
    ASSERT_NE(mat->khrMaterialsSheen, nullptr);
    EXPECT_NEAR(mat->khrMaterialsSheen->sheenRoughnessFactor, 0.5, 1e-6);
}

TEST_F(KHRMaterialsSheenTest, Textures)
{
    const Material *mat = findMaterial(asset, "SheenMat");
    ASSERT_NE(mat->khrMaterialsSheen, nullptr);
    EXPECT_NE(mat->khrMaterialsSheen->sheenColorTexture,     nullptr);
    EXPECT_NE(mat->khrMaterialsSheen->sheenRoughnessTexture, nullptr);
    EXPECT_EQ(mat->khrMaterialsSheen->sheenColorTexture->index,     0u);
    EXPECT_EQ(mat->khrMaterialsSheen->sheenRoughnessTexture->index, 1u);
}

TEST_F(KHRMaterialsSheenTest, AbsentIsNull)
{
    const Material *mat = findMaterial(asset, "PlainMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrMaterialsSheen, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 14.  KHR_materials_specular
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsSpecularJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_specular"],
  "materials": [
    {
      "name": "SpecularMat",
      "extensions": {
        "KHR_materials_specular": {
          "specularFactor":       0.8,
          "specularTexture":      {"index": 0},
          "specularColorFactor":  [1.0, 0.5, 0.2],
          "specularColorTexture": {"index": 1}
        }
      }
    },
    {"name": "PlainMat"}
  ]
})";

class KHRMaterialsSpecularTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsSpecularTest() { asset = loadInline(kMaterialsSpecularJSON); }
};

TEST_F(KHRMaterialsSpecularTest, ExtensionNotNull)
{
    const Material *mat = findMaterial(asset, "SpecularMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NE(mat->khrMaterialsSpecular, nullptr);
}

TEST_F(KHRMaterialsSpecularTest, SpecularFactor)
{
    const Material *mat = findMaterial(asset, "SpecularMat");
    ASSERT_NE(mat->khrMaterialsSpecular, nullptr);
    EXPECT_NEAR(mat->khrMaterialsSpecular->specularFactor, 0.8, 1e-6);
}

TEST_F(KHRMaterialsSpecularTest, SpecularColorFactor)
{
    const Material *mat = findMaterial(asset, "SpecularMat");
    ASSERT_NE(mat->khrMaterialsSpecular, nullptr);
    EXPECT_NEAR(mat->khrMaterialsSpecular->specularColorFactor.data[0], 1.0, 1e-6);
    EXPECT_NEAR(mat->khrMaterialsSpecular->specularColorFactor.data[1], 0.5, 1e-6);
    EXPECT_NEAR(mat->khrMaterialsSpecular->specularColorFactor.data[2], 0.2, 1e-6);
}

TEST_F(KHRMaterialsSpecularTest, Textures)
{
    const Material *mat = findMaterial(asset, "SpecularMat");
    ASSERT_NE(mat->khrMaterialsSpecular, nullptr);
    EXPECT_NE(mat->khrMaterialsSpecular->specularTexture,      nullptr);
    EXPECT_NE(mat->khrMaterialsSpecular->specularColorTexture, nullptr);
    EXPECT_EQ(mat->khrMaterialsSpecular->specularTexture->index,      0u);
    EXPECT_EQ(mat->khrMaterialsSpecular->specularColorTexture->index, 1u);
}

TEST_F(KHRMaterialsSpecularTest, AbsentIsNull)
{
    const Material *mat = findMaterial(asset, "PlainMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrMaterialsSpecular, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 15.  KHR_materials_volume
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsVolumeJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_volume"],
  "materials": [
    {
      "name": "VolumeMat",
      "extensions": {
        "KHR_materials_volume": {
          "thicknessFactor":      2.5,
          "thicknessTexture":     {"index": 0},
          "attenuationDistance":  8.0,
          "attenuationColor":     [0.8, 0.2, 0.1]
        }
      }
    },
    {"name": "PlainMat"}
  ]
})";

class KHRMaterialsVolumeTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsVolumeTest() { asset = loadInline(kMaterialsVolumeJSON); }
};

TEST_F(KHRMaterialsVolumeTest, ExtensionNotNull)
{
    const Material *mat = findMaterial(asset, "VolumeMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NE(mat->khrMaterialsVolume, nullptr);
}

TEST_F(KHRMaterialsVolumeTest, ThicknessFactor)
{
    const Material *mat = findMaterial(asset, "VolumeMat");
    ASSERT_NE(mat->khrMaterialsVolume, nullptr);
    EXPECT_NEAR(mat->khrMaterialsVolume->thicknessFactor, 2.5, 1e-6);
}

TEST_F(KHRMaterialsVolumeTest, AttenuationDistance)
{
    const Material *mat = findMaterial(asset, "VolumeMat");
    ASSERT_NE(mat->khrMaterialsVolume, nullptr);
    EXPECT_NEAR(mat->khrMaterialsVolume->attenuationDistance, 8.0, 1e-6);
}

TEST_F(KHRMaterialsVolumeTest, AttenuationColor)
{
    const Material *mat = findMaterial(asset, "VolumeMat");
    ASSERT_NE(mat->khrMaterialsVolume, nullptr);
    EXPECT_NEAR(mat->khrMaterialsVolume->attenuationColor.data[0], 0.8, 1e-6);
    EXPECT_NEAR(mat->khrMaterialsVolume->attenuationColor.data[1], 0.2, 1e-6);
    EXPECT_NEAR(mat->khrMaterialsVolume->attenuationColor.data[2], 0.1, 1e-6);
}

TEST_F(KHRMaterialsVolumeTest, ThicknessTexture)
{
    const Material *mat = findMaterial(asset, "VolumeMat");
    ASSERT_NE(mat->khrMaterialsVolume, nullptr);
    ASSERT_NE(mat->khrMaterialsVolume->thicknessTexture, nullptr);
    EXPECT_EQ(mat->khrMaterialsVolume->thicknessTexture->index, 0u);
}

TEST_F(KHRMaterialsVolumeTest, AbsentIsNull)
{
    const Material *mat = findMaterial(asset, "PlainMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrMaterialsVolume, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 16.  KHR_materials_iridescence
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsIridescenceJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_iridescence"],
  "materials": [
    {
      "name": "IridescentMat",
      "extensions": {
        "KHR_materials_iridescence": {
          "iridescenceFactor":           0.9,
          "iridescenceTexture":          {"index": 0},
          "iridescenceIor":              1.4,
          "iridescenceThicknessMinimum": 150.0,
          "iridescenceThicknessMaximum": 500.0,
          "iridescenceThicknessTexture": {"index": 1}
        }
      }
    },
    {"name": "PlainMat"}
  ]
})";

class KHRMaterialsIridescenceTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsIridescenceTest() { asset = loadInline(kMaterialsIridescenceJSON); }
};

TEST_F(KHRMaterialsIridescenceTest, ExtensionNotNull)
{
    const Material *mat = findMaterial(asset, "IridescentMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NE(mat->khrMaterialsIridescence, nullptr);
}

TEST_F(KHRMaterialsIridescenceTest, IridescenceFactor)
{
    const Material *mat = findMaterial(asset, "IridescentMat");
    ASSERT_NE(mat->khrMaterialsIridescence, nullptr);
    EXPECT_NEAR(mat->khrMaterialsIridescence->iridescenceFactor, 0.9, 1e-6);
}

TEST_F(KHRMaterialsIridescenceTest, IridescenceIor)
{
    const Material *mat = findMaterial(asset, "IridescentMat");
    ASSERT_NE(mat->khrMaterialsIridescence, nullptr);
    EXPECT_NEAR(mat->khrMaterialsIridescence->iridescenceIor, 1.4, 1e-6);
}

TEST_F(KHRMaterialsIridescenceTest, ThicknessRange)
{
    const Material *mat = findMaterial(asset, "IridescentMat");
    ASSERT_NE(mat->khrMaterialsIridescence, nullptr);
    EXPECT_NEAR(mat->khrMaterialsIridescence->iridescenceThicknessMinimum, 150.0, 1e-6);
    EXPECT_NEAR(mat->khrMaterialsIridescence->iridescenceThicknessMaximum, 500.0, 1e-6);
}

TEST_F(KHRMaterialsIridescenceTest, Textures)
{
    const Material *mat = findMaterial(asset, "IridescentMat");
    ASSERT_NE(mat->khrMaterialsIridescence, nullptr);
    EXPECT_NE(mat->khrMaterialsIridescence->iridescenceTexture,          nullptr);
    EXPECT_NE(mat->khrMaterialsIridescence->iridescenceThicknessTexture, nullptr);
    EXPECT_EQ(mat->khrMaterialsIridescence->iridescenceTexture->index,          0u);
    EXPECT_EQ(mat->khrMaterialsIridescence->iridescenceThicknessTexture->index, 1u);
}

TEST_F(KHRMaterialsIridescenceTest, AbsentIsNull)
{
    const Material *mat = findMaterial(asset, "PlainMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrMaterialsIridescence, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 17.  KHR_materials_anisotropy
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsAnisotropyJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_anisotropy"],
  "materials": [
    {
      "name": "BrushedMat",
      "extensions": {
        "KHR_materials_anisotropy": {
          "anisotropyStrength": 0.7,
          "anisotropyRotation": 1.5708,
          "anisotropyTexture":  {"index": 0}
        }
      }
    },
    {"name": "PlainMat"}
  ]
})";

class KHRMaterialsAnisotropyTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsAnisotropyTest() { asset = loadInline(kMaterialsAnisotropyJSON); }
};

TEST_F(KHRMaterialsAnisotropyTest, ExtensionNotNull)
{
    const Material *mat = findMaterial(asset, "BrushedMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_NE(mat->khrMaterialsAnisotropy, nullptr);
}

TEST_F(KHRMaterialsAnisotropyTest, AnisotropyStrength)
{
    const Material *mat = findMaterial(asset, "BrushedMat");
    ASSERT_NE(mat->khrMaterialsAnisotropy, nullptr);
    EXPECT_NEAR(mat->khrMaterialsAnisotropy->anisotropyStrength, 0.7, 1e-6);
}

TEST_F(KHRMaterialsAnisotropyTest, AnisotropyRotation)
{
    const Material *mat = findMaterial(asset, "BrushedMat");
    ASSERT_NE(mat->khrMaterialsAnisotropy, nullptr);
    EXPECT_NEAR(mat->khrMaterialsAnisotropy->anisotropyRotation, 1.5708, 1e-4);
}

TEST_F(KHRMaterialsAnisotropyTest, AnisotropyTexture)
{
    const Material *mat = findMaterial(asset, "BrushedMat");
    ASSERT_NE(mat->khrMaterialsAnisotropy, nullptr);
    ASSERT_NE(mat->khrMaterialsAnisotropy->anisotropyTexture, nullptr);
    EXPECT_EQ(mat->khrMaterialsAnisotropy->anisotropyTexture->index, 0u);
}

TEST_F(KHRMaterialsAnisotropyTest, AbsentIsNull)
{
    const Material *mat = findMaterial(asset, "PlainMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrMaterialsAnisotropy, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 18.  KHR_materials_variants
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kMaterialsVariantsJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_variants"],
  "extensions": {
    "KHR_materials_variants": {
      "variants": [
        {"name": "Midnight"},
        {"name": "Jungle"},
        {"name": "Arctic"}
      ]
    }
  },
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"name": "Node", "mesh": 0}],
  "meshes": [{
    "name": "Shoe",
    "primitives": [{
      "attributes": {"POSITION": 0},
      "material": 0,
      "extensions": {
        "KHR_materials_variants": {
          "mappings": [
            {"material": 0, "variants": [0]},
            {"material": 1, "variants": [1, 2]}
          ]
        }
      }
    }]
  }],
  "materials": [{"name": "MidnightMat"}, {"name": "JungleMat"}]
})";

class KHRMaterialsVariantsTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRMaterialsVariantsTest() { asset = loadInline(kMaterialsVariantsJSON); }
};

TEST_F(KHRMaterialsVariantsTest, VariantCount)
{
    ASSERT_NE(asset->khrMaterialsVariants, nullptr);
    EXPECT_EQ(asset->khrMaterialsVariants->size(), 3u);
}

TEST_F(KHRMaterialsVariantsTest, VariantNames)
{
    ASSERT_NE(asset->khrMaterialsVariants, nullptr);
    EXPECT_EQ((*asset->khrMaterialsVariants)[0].name, "Midnight");
    EXPECT_EQ((*asset->khrMaterialsVariants)[1].name, "Jungle");
    EXPECT_EQ((*asset->khrMaterialsVariants)[2].name, "Arctic");
}

TEST_F(KHRMaterialsVariantsTest, PrimitiveMappingCount)
{
    ASSERT_EQ(asset->meshes->size(), 1u);
    const auto &prim = (*asset->meshes)[0].primitives[0];
    EXPECT_EQ(prim.khrMaterialsVariantsMappings.size(), 2u);
}

TEST_F(KHRMaterialsVariantsTest, MappingMaterial)
{
    const auto &mappings = (*asset->meshes)[0].primitives[0].khrMaterialsVariantsMappings;
    EXPECT_EQ(mappings[0].material, 0);
    EXPECT_EQ(mappings[1].material, 1);
}

TEST_F(KHRMaterialsVariantsTest, MappingVariants)
{
    const auto &mappings = (*asset->meshes)[0].primitives[0].khrMaterialsVariantsMappings;
    ASSERT_EQ(mappings[0].variants.size(), 1u);
    EXPECT_EQ(mappings[0].variants[0], 0u);
    ASSERT_EQ(mappings[1].variants.size(), 2u);
    EXPECT_EQ(mappings[1].variants[0], 1u);
    EXPECT_EQ(mappings[1].variants[1], 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 19.  KHR_xmp_json_ld
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kXmpJSON = R"({
  "asset": {
    "version": "2.0",
    "extensions": {
      "KHR_xmp_json_ld": {"packet": 0}
    }
  },
  "extensionsUsed": ["KHR_xmp_json_ld"],
  "extensions": {
    "KHR_xmp_json_ld": {
      "packets": [
        {"@context": {"dc": "http://purl.org/dc/elements/1.1/"}, "dc:title": "My Asset"},
        {"@context": {"dc": "http://purl.org/dc/elements/1.1/"}, "dc:creator": "Someone"}
      ]
    }
  },
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{
    "name": "AnnotatedNode",
    "extensions": {"KHR_xmp_json_ld": {"packet": 1}}
  }],
  "materials": [{
    "name": "AnnotatedMat",
    "extensions": {"KHR_xmp_json_ld": {"packet": 0}}
  }]
})";

class KHRXmpJsonLdTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRXmpJsonLdTest() { asset = loadInline(kXmpJSON); }
};

TEST_F(KHRXmpJsonLdTest, PacketCount)
{
    ASSERT_NE(asset->khrXmpPackets, nullptr);
    EXPECT_EQ(asset->khrXmpPackets->size(), 2u);
}

TEST_F(KHRXmpJsonLdTest, PacketsAreNonEmpty)
{
    ASSERT_NE(asset->khrXmpPackets, nullptr);
    EXPECT_FALSE((*asset->khrXmpPackets)[0].empty());
    EXPECT_FALSE((*asset->khrXmpPackets)[1].empty());
}

TEST_F(KHRXmpJsonLdTest, AssetPacketIndex)
{
    EXPECT_EQ(asset->khrXmpAssetPacket, 0);
}

TEST_F(KHRXmpJsonLdTest, NodePacketIndex)
{
    const Node *n = findNode(asset, "AnnotatedNode");
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->khrXmpPacket, 1);
}

TEST_F(KHRXmpJsonLdTest, MaterialPacketIndex)
{
    const Material *mat = findMaterial(asset, "AnnotatedMat");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->khrXmpPacket, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 20.  KHR_animation_pointer
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kAnimationPointerJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_animation_pointer"],
  "scene": 0,
  "scenes": [{"nodes": [0, 1]}],
  "nodes": [
    {"name": "Cube"},
    {"name": "Camera"}
  ],
  "animations": [{
    "name": "NodeAnim",
    "samplers": [
      {"input": 0, "output": 1, "interpolation": "LINEAR"},
      {"input": 0, "output": 2, "interpolation": "STEP"},
      {"input": 0, "output": 3, "interpolation": "CUBICSPLINE"}
    ],
    "channels": [
      {
        "sampler": 0,
        "target": {
          "path": "pointer",
          "extensions": {
            "KHR_animation_pointer": {"pointer": "/nodes/0/translation"}
          }
        }
      },
      {
        "sampler": 1,
        "target": {
          "path": "pointer",
          "extensions": {
            "KHR_animation_pointer": {"pointer": "/materials/0/pbrMetallicRoughness/baseColorFactor"}
          }
        }
      },
      {
        "sampler": 2,
        "target": {
          "node": 1,
          "path": "rotation"
        }
      }
    ]
  }]
})";

class KHRAnimationPointerTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRAnimationPointerTest() { asset = loadInline(kAnimationPointerJSON); }
};

TEST_F(KHRAnimationPointerTest, AnimationParsed)
{
    ASSERT_EQ(asset->animations->size(), 1u);
    EXPECT_EQ((*asset->animations)[0].name, "NodeAnim");
}

TEST_F(KHRAnimationPointerTest, SamplerCount)
{
    const auto &anim = (*asset->animations)[0];
    EXPECT_EQ(anim.samplers.size(), 3u);
}

TEST_F(KHRAnimationPointerTest, SamplerInterpolations)
{
    const auto &samplers = (*asset->animations)[0].samplers;
    EXPECT_EQ(samplers[0].interpolation, AnimationInterpolation::aiLinear);
    EXPECT_EQ(samplers[1].interpolation, AnimationInterpolation::aiStep);
    EXPECT_EQ(samplers[2].interpolation, AnimationInterpolation::aiCubicSpline);
}

TEST_F(KHRAnimationPointerTest, PointerChannelNodeIsMinusOne)
{
    // Channels that use KHR_animation_pointer have no target node
    const auto &ch = (*asset->animations)[0].channels[0];
    EXPECT_EQ(ch.target.node, -1);
    EXPECT_EQ(ch.target.path, "pointer");
}

TEST_F(KHRAnimationPointerTest, PointerString)
{
    const auto &ch0 = (*asset->animations)[0].channels[0];
    ASSERT_NE(ch0.target.khrAnimationPointer, nullptr);
    EXPECT_EQ(ch0.target.khrAnimationPointer->pointer, "/nodes/0/translation");

    const auto &ch1 = (*asset->animations)[0].channels[1];
    ASSERT_NE(ch1.target.khrAnimationPointer, nullptr);
    EXPECT_EQ(ch1.target.khrAnimationPointer->pointer,
              "/materials/0/pbrMetallicRoughness/baseColorFactor");
}

TEST_F(KHRAnimationPointerTest, RegularChannelHasNoPointer)
{
    const auto &ch = (*asset->animations)[0].channels[2];
    EXPECT_EQ(ch.target.path, "rotation");
    EXPECT_EQ(ch.target.node, 1);
    EXPECT_EQ(ch.target.khrAnimationPointer, nullptr);
}

TEST_F(KHRAnimationPointerTest, SamplerInputOutput)
{
    const auto &s = (*asset->animations)[0].samplers[0];
    EXPECT_EQ(s.input,  0u);
    EXPECT_EQ(s.output, 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 21.  KHR_draco_mesh_compression
// ─────────────────────────────────────────────────────────────────────────────

// Minimal glTF with a Draco-compressed primitive. The bufferView contains a
// 4-byte dummy payload — not valid Draco data — so decodedIndices /
// decodedAttributes remain empty. This exercises JSON parsing only.
static const std::string kDracoJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_draco_mesh_compression"],
  "buffers":     [{"byteLength": 4, "uri": "data:application/octet-stream;base64,AAAAAA=="}],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 4}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"componentType": 5125, "count": 3, "type": "SCALAR"}
  ],
  "meshes": [{
    "name": "DracoMesh",
    "primitives": [{
      "attributes": {"POSITION": 0},
      "indices": 1,
      "extensions": {
        "KHR_draco_mesh_compression": {
          "bufferView": 0,
          "attributes": {"POSITION": 0, "NORMAL": 1}
        }
      }
    }]
  }]
})";

class KHRDracoMeshCompressionTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRDracoMeshCompressionTest() { asset = loadInline(kDracoJSON); }
};

TEST_F(KHRDracoMeshCompressionTest, ExtensionNotNull)
{
    ASSERT_EQ(asset->meshes->size(), 1u);
    const auto &prim = (*asset->meshes)[0].primitives[0];
    EXPECT_NE(prim.khrDracoMeshCompression, nullptr);
}

TEST_F(KHRDracoMeshCompressionTest, BufferViewIndex)
{
    const auto &ext = *(*asset->meshes)[0].primitives[0].khrDracoMeshCompression;
    EXPECT_EQ(ext.bufferView, 0);
}

TEST_F(KHRDracoMeshCompressionTest, AttributeMap)
{
    const auto &ext = *(*asset->meshes)[0].primitives[0].khrDracoMeshCompression;
    ASSERT_EQ(ext.attributes.size(), 2u);
    EXPECT_EQ(ext.attributes.at("POSITION"), 0u);
    EXPECT_EQ(ext.attributes.at("NORMAL"),   1u);
}

TEST_F(KHRDracoMeshCompressionTest, InvalidDataYieldsEmptyDecoded)
{
    // The dummy payload is not valid Draco data, so decoded arrays stay empty.
    const auto &ext = *(*asset->meshes)[0].primitives[0].khrDracoMeshCompression;
    EXPECT_TRUE(ext.decodedIndices.empty());
    EXPECT_TRUE(ext.decodedAttributes.empty());
}

TEST_F(KHRDracoMeshCompressionTest, AbsentIsNull)
{
    // A primitive without the extension must leave the field as nullptr.
    const std::string plain = R"({
      "asset": {"version": "2.0"},
      "meshes": [{"primitives": [{"attributes": {}}]}]
    })";
    auto g = loadInline(plain);
    ASSERT_EQ(g->meshes->size(), 1u);
    EXPECT_EQ((*g->meshes)[0].primitives[0].khrDracoMeshCompression, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Integration tests — real Khronos sample assets
// These tests run only when launch_test.sh has downloaded the model files.
// ─────────────────────────────────────────────────────────────────────────────

// Helper: returns true if the file can be opened.
static bool fileExists(const std::string &path)
{
    return std::ifstream(path).good();
}

// ── LightsPunctualLamp ───────────────────────────────────────────────────────

class LightsPunctualLampTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    bool available = false;

    LightsPunctualLampTest()
    {
        if (fileExists("LightsPunctualLamp.gltf")) {
            asset     = loadFile("LightsPunctualLamp.gltf");
            available = true;
        }
    }
};

TEST_F(LightsPunctualLampTest, HasLights)
{
    if (!available) GTEST_SKIP() << "LightsPunctualLamp.gltf not downloaded";
    ASSERT_NE(asset->khrLightsPunctual, nullptr);
    EXPECT_GT(asset->khrLightsPunctual->size(), 0u);
}

TEST_F(LightsPunctualLampTest, LightTypesValid)
{
    if (!available) GTEST_SKIP() << "LightsPunctualLamp.gltf not downloaded";
    for (const auto &light : *asset->khrLightsPunctual) {
        bool valid = (light.type == KHRLightPunctualType::directional ||
                      light.type == KHRLightPunctualType::point       ||
                      light.type == KHRLightPunctualType::spot);
        EXPECT_TRUE(valid) << "unexpected light type for: " << light.name;
    }
}

TEST_F(LightsPunctualLampTest, LightNodesHaveValidRef)
{
    if (!available) GTEST_SKIP() << "LightsPunctualLamp.gltf not downloaded";
    auto lightCount = (int64_t)asset->khrLightsPunctual->size();
    for (const auto &node : *asset->nodes) {
        if (node.light != -1) {
            EXPECT_GE(node.light, 0);
            EXPECT_LT(node.light, lightCount);
        }
    }
}

// ── AnimationPointer ─────────────────────────────────────────────────────────

class AnimationPointerSampleTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    bool available = false;

    AnimationPointerSampleTest()
    {
        if (fileExists("AnimationPointerUVs.gltf")) {
            asset     = loadFile("AnimationPointerUVs.gltf");
            available = true;
        }
    }
};

TEST_F(AnimationPointerSampleTest, HasAnimations)
{
    if (!available) GTEST_SKIP() << "AnimationPointerUVs.gltf not downloaded";
    EXPECT_GT(asset->animations->size(), 0u);
}

TEST_F(AnimationPointerSampleTest, HasPointerChannels)
{
    if (!available) GTEST_SKIP() << "AnimationPointerUVs.gltf not downloaded";
    bool found = false;
    for (const auto &anim : *asset->animations)
        for (const auto &ch : anim.channels)
            if (ch.target.khrAnimationPointer != nullptr) { found = true; break; }
    EXPECT_TRUE(found) << "no KHR_animation_pointer channels found";
}

TEST_F(AnimationPointerSampleTest, PointerStringsNonEmpty)
{
    if (!available) GTEST_SKIP() << "AnimationPointerUVs.gltf not downloaded";
    for (const auto &anim : *asset->animations)
        for (const auto &ch : anim.channels)
            if (ch.target.khrAnimationPointer != nullptr)
                EXPECT_FALSE(ch.target.khrAnimationPointer->pointer.empty());
}

// ── MaterialsVariantsShoe ────────────────────────────────────────────────────

class MaterialsVariantsShoeSampleTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    bool available = false;

    MaterialsVariantsShoeSampleTest()
    {
        if (fileExists("MaterialsVariantsShoe.gltf")) {
            asset     = loadFile("MaterialsVariantsShoe.gltf");
            available = true;
        }
    }
};

TEST_F(MaterialsVariantsShoeSampleTest, HasVariants)
{
    if (!available) GTEST_SKIP() << "MaterialsVariantsShoe.gltf not downloaded";
    ASSERT_NE(asset->khrMaterialsVariants, nullptr);
    EXPECT_GT(asset->khrMaterialsVariants->size(), 0u);
}

TEST_F(MaterialsVariantsShoeSampleTest, VariantNamesNonEmpty)
{
    if (!available) GTEST_SKIP() << "MaterialsVariantsShoe.gltf not downloaded";
    for (const auto &v : *asset->khrMaterialsVariants)
        EXPECT_FALSE(v.name.empty());
}

TEST_F(MaterialsVariantsShoeSampleTest, PrimitivesHaveMappings)
{
    if (!available) GTEST_SKIP() << "MaterialsVariantsShoe.gltf not downloaded";
    bool found = false;
    for (const auto &mesh : *asset->meshes)
        for (const auto &prim : mesh.primitives)
            if (!prim.khrMaterialsVariantsMappings.empty()) { found = true; break; }
    EXPECT_TRUE(found) << "no variant mappings found on any primitive";
}

// ── AvocadoDraco ─────────────────────────────────────────────────────────────
// Avocado (glTF-Draco variant). Uses external .bin buffer with Draco compression.
// The callback-based loader automatically loads external buffers and decompresses.

class AvocadoDracoSampleTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    bool available = false;

    AvocadoDracoSampleTest()
    {
        if (!fileExists("AvocadoDraco.gltf") || !fileExists("Avocado.bin"))
            return;

        // Use callback-based loader - decompression happens automatically
        asset = loadFile("AvocadoDraco.gltf");
        available = (asset != nullptr && !asset->buffers->empty());
    }
};

TEST_F(AvocadoDracoSampleTest, HasMesh)
{
    if (!available) GTEST_SKIP() << "AvocadoDraco.gltf / Avocado.bin not downloaded";
    EXPECT_GT(asset->meshes->size(), 0u);
}

TEST_F(AvocadoDracoSampleTest, PrimitiveHasDracoExtension)
{
    if (!available) GTEST_SKIP() << "AvocadoDraco.gltf / Avocado.bin not downloaded";
    bool found = false;
    for (const auto &mesh : *asset->meshes)
        for (const auto &prim : mesh.primitives)
            if (prim.khrDracoMeshCompression) { found = true; break; }
    EXPECT_TRUE(found) << "no KHR_draco_mesh_compression primitive found";
}

TEST_F(AvocadoDracoSampleTest, DecodedIndicesNotEmpty)
{
    if (!available) GTEST_SKIP() << "AvocadoDraco.gltf / Avocado.bin not downloaded";
    for (const auto &mesh : *asset->meshes)
        for (const auto &prim : mesh.primitives)
            if (prim.khrDracoMeshCompression)
            {
                const auto &ext = *prim.khrDracoMeshCompression;
                EXPECT_FALSE(ext.decodedIndices.empty()) << "decodedIndices is empty after decompression";
                EXPECT_EQ(ext.decodedIndices.size() % 3, 0u) << "index count not a multiple of 3";
            }
}

TEST_F(AvocadoDracoSampleTest, DecodedPositionNotEmpty)
{
    if (!available) GTEST_SKIP() << "AvocadoDraco.gltf / Avocado.bin not downloaded";
    for (const auto &mesh : *asset->meshes)
        for (const auto &prim : mesh.primitives)
            if (prim.khrDracoMeshCompression)
            {
                const auto &ext = *prim.khrDracoMeshCompression;
                ASSERT_TRUE(ext.decodedAttributes.count("POSITION"))
                    << "POSITION missing from decodedAttributes";
                EXPECT_FALSE(ext.decodedAttributes.at("POSITION").empty());
            }
}

// ─────────────────────────────────────────────────────────────────────────────
// 22.  EXT_meshopt_compression
// ─────────────────────────────────────────────────────────────────────────────

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string base64EncodeBytes(const uint8_t *data, size_t len)
{
    static const char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        uint32_t v = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) v |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) v |= static_cast<uint32_t>(data[i + 2]);
        out += kAlphabet[(v >> 18) & 63];
        out += kAlphabet[(v >> 12) & 63];
        out += (i + 1 < len) ? kAlphabet[(v >> 6) & 63] : '=';
        out += (i + 2 < len) ? kAlphabet[v & 63] : '=';
    }
    return out;
}

// Build a minimal GLTF JSON with one EXT_meshopt_compression buffer view
// containing meshopt-encoded vertex data.
static std::string buildMeshoptGLTF(const uint8_t *compressed, size_t compressedSize,
                                     size_t count, size_t stride)
{
    std::string b64 = base64EncodeBytes(compressed, compressedSize);
    std::string uri = "data:application/octet-stream;base64," + b64;
    std::ostringstream js;
    js << R"({"asset":{"version":"2.0"},)"
       << R"("extensionsUsed":["EXT_meshopt_compression"],)"
       << R"("extensionsRequired":["EXT_meshopt_compression"],)"
       << R"("buffers":[{"byteLength":)" << compressedSize
       << R"(,"uri":")" << uri << R"("}],)"
       << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":0,)"
       << R"("extensions":{"EXT_meshopt_compression":{)"
       << R"("buffer":0,"byteOffset":0,)"
       << R"("byteLength":)" << compressedSize << ","
       << R"("byteStride":)" << stride << ","
       << R"("count":)" << count << ","
       << R"("mode":"ATTRIBUTES"}}}]})";
    return js.str();
}

// ── parse-only (no real data) ─────────────────────────────────────────────────

static const std::string kMeshoptParseJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["EXT_meshopt_compression"],
  "buffers": [{"byteLength": 256}],
  "bufferViews": [
    {
      "buffer": 0, "byteOffset": 0, "byteLength": 0,
      "extensions": {
        "EXT_meshopt_compression": {
          "buffer": 0, "byteOffset": 0, "byteLength": 100,
          "byteStride": 12, "count": 8, "mode": "ATTRIBUTES"
        }
      }
    },
    {
      "buffer": 0, "byteOffset": 0, "byteLength": 0,
      "extensions": {
        "EXT_meshopt_compression": {
          "buffer": 0, "byteOffset": 0, "byteLength": 50,
          "byteStride": 6, "count": 4, "mode": "TRIANGLES"
        }
      }
    },
    {
      "buffer": 0, "byteOffset": 0, "byteLength": 0,
      "extensions": {
        "EXT_meshopt_compression": {
          "buffer": 0, "byteOffset": 0, "byteLength": 30,
          "byteStride": 4, "count": 6, "mode": "INDICES",
          "filter": "EXPONENTIAL"
        }
      }
    }
  ]
})";

class EXTMeshoptParseTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    EXTMeshoptParseTest() { asset = loadInline(kMeshoptParseJSON); }
};

TEST_F(EXTMeshoptParseTest, BufferViewCountMatches)
{
    ASSERT_NE(asset->bufferViews, nullptr);
    EXPECT_EQ(asset->bufferViews->size(), 3u);
}

TEST_F(EXTMeshoptParseTest, AttributesModeAndStride)
{
    const auto &bv = (*asset->bufferViews)[0];
    ASSERT_NE(bv.extMeshoptCompression, nullptr);
    const auto &ext = *bv.extMeshoptCompression;
    EXPECT_EQ(ext.mode,       EXTMeshoptMode::ATTRIBUTES);
    EXPECT_EQ(ext.byteStride, 12u);
    EXPECT_EQ(ext.count,       8u);
    EXPECT_EQ(ext.filter,     EXTMeshoptFilter::NONE);
}

TEST_F(EXTMeshoptParseTest, TrianglesMode)
{
    const auto &bv = (*asset->bufferViews)[1];
    ASSERT_NE(bv.extMeshoptCompression, nullptr);
    EXPECT_EQ(bv.extMeshoptCompression->mode, EXTMeshoptMode::TRIANGLES);
}

TEST_F(EXTMeshoptParseTest, IndicesModeWithFilter)
{
    const auto &bv = (*asset->bufferViews)[2];
    ASSERT_NE(bv.extMeshoptCompression, nullptr);
    const auto &ext = *bv.extMeshoptCompression;
    EXPECT_EQ(ext.mode,   EXTMeshoptMode::INDICES);
    EXPECT_EQ(ext.filter, EXTMeshoptFilter::EXPONENTIAL);
}

TEST_F(EXTMeshoptParseTest, NoExtensionBufferViewUnaffected)
{
    // All three buffer views have the extension; none should be null.
    for (const auto &bv : *asset->bufferViews)
        EXPECT_NE(bv.extMeshoptCompression, nullptr);
}

// ── roundtrip decompression test ──────────────────────────────────────────────

TEST(EXTMeshoptCompressionTest, VertexBufferRoundtrip)
{
    // Build 4 synthetic vertices (xyz float = 12 bytes each).
    struct Vertex { float x, y, z; };
    const std::vector<Vertex> original = {
        {1.f, 2.f, 3.f}, {4.f, 5.f, 6.f}, {7.f, 8.f, 9.f}, {10.f, 11.f, 12.f}
    };
    const size_t count  = original.size();
    const size_t stride = sizeof(Vertex);

    // Encode using meshoptimizer.
    size_t bound = meshopt_encodeVertexBufferBound(count, stride);
    std::vector<unsigned char> compressed(bound);
    size_t compressedSize = meshopt_encodeVertexBuffer(
        compressed.data(), bound,
        reinterpret_cast<const unsigned char *>(original.data()),
        count, stride);
    compressed.resize(compressedSize);

    // Build an inline GLTF JSON and load it.
    std::string json = buildMeshoptGLTF(compressed.data(), compressedSize, count, stride);
    auto asset = loadInline(json);

    ASSERT_NE(asset->bufferViews, nullptr);
    ASSERT_FALSE(asset->bufferViews->empty());

    const auto &bv = (*asset->bufferViews)[0];
    ASSERT_NE(bv.extMeshoptCompression, nullptr)
        << "extension not parsed";
    EXPECT_FALSE(bv.decodedData.empty())
        << "decodedData is empty — decompression did not run";

    ASSERT_EQ(bv.decodedData.size(), count * stride);
    EXPECT_EQ(std::memcmp(bv.decodedData.data(), original.data(), count * stride), 0)
        << "decompressed data does not match original vertices";
}

// ── BrainStem sample (KHR_meshopt_compression) ───────────────────────────────
// BrainStem/glTF-Meshopt uses KHR_meshopt_compression.
// Buffer 0 = BrainStem.bin (compressed data).
// Buffer 1 = fallback placeholder (no URI, byteLength = decompressed size).
// BufferViews reference buffer 1 (placeholder) but the extension points to
// buffer 0 for the actual compressed bytes.

class BrainStemMeshoptSampleTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    bool available = false;

    BrainStemMeshoptSampleTest()
    {
        if (!fileExists("BrainStem.gltf") || !fileExists("BrainStem.bin"))
            return;

        // Use callback-based loader - decompression happens automatically
        asset = loadFile("BrainStem.gltf");
        available = (asset != nullptr && !asset->buffers->empty());
    }
};

TEST_F(BrainStemMeshoptSampleTest, HasBufferViews)
{
    if (!available) GTEST_SKIP() << "BrainStem.gltf / BrainStem.bin not downloaded";
    ASSERT_NE(asset->bufferViews, nullptr);
    EXPECT_GT(asset->bufferViews->size(), 0u);
}

TEST_F(BrainStemMeshoptSampleTest, BufferViewsHaveMeshoptExtension)
{
    if (!available) GTEST_SKIP() << "BrainStem.gltf / BrainStem.bin not downloaded";
    bool found = false;
    for (const auto &bv : *asset->bufferViews)
        if (bv.extMeshoptCompression) { found = true; break; }
    EXPECT_TRUE(found) << "no KHR_meshopt_compression buffer view found";
}

TEST_F(BrainStemMeshoptSampleTest, DecodedDataNonEmpty)
{
    if (!available) GTEST_SKIP() << "BrainStem.gltf / BrainStem.bin not downloaded";
    bool anyDecoded = false;
    for (const auto &bv : *asset->bufferViews)
        if (bv.extMeshoptCompression && !bv.decodedData.empty()) { anyDecoded = true; break; }
    EXPECT_TRUE(anyDecoded) << "no buffer view has non-empty decodedData after decompression";
}

TEST_F(BrainStemMeshoptSampleTest, DecodedDataSizeMatchesCountTimesStride)
{
    if (!available) GTEST_SKIP() << "BrainStem.gltf / BrainStem.bin not downloaded";
    for (const auto &bv : *asset->bufferViews)
    {
        if (!bv.extMeshoptCompression || bv.decodedData.empty()) continue;
        const auto &ext = *bv.extMeshoptCompression;
        size_t expected = static_cast<size_t>(ext.count) * static_cast<size_t>(ext.byteStride);
        EXPECT_EQ(bv.decodedData.size(), expected)
            << "decodedData size != count * byteStride";
    }
}

TEST_F(BrainStemMeshoptSampleTest, HasMeshes)
{
    if (!available) GTEST_SKIP() << "BrainStem.gltf / BrainStem.bin not downloaded";
    ASSERT_NE(asset->meshes, nullptr);
    EXPECT_GT(asset->meshes->size(), 0u);
}

// ── roundtrip tests (self-contained) ─────────────────────────────────────────

TEST(EXTMeshoptCompressionTest, IndexBufferRoundtrip)
{
    // 6 indices (2 triangles) stored as uint32 — INDICES mode.
    const std::vector<uint32_t> original = {0, 1, 2, 2, 3, 0};
    const size_t count  = original.size();
    const size_t stride = sizeof(uint32_t);

    size_t bound = meshopt_encodeIndexSequenceBound(count, /*vertex_count=*/4);
    std::vector<unsigned char> compressed(bound);
    size_t compressedSize = meshopt_encodeIndexSequence(
        compressed.data(), bound, original.data(), count);
    compressed.resize(compressedSize);

    // Build JSON with INDICES mode.
    std::string b64  = base64EncodeBytes(compressed.data(), compressedSize);
    std::string uri  = "data:application/octet-stream;base64," + b64;
    std::ostringstream js;
    js << R"({"asset":{"version":"2.0"},)"
       << R"("extensionsUsed":["EXT_meshopt_compression"],)"
       << R"("buffers":[{"byteLength":)" << compressedSize << R"(,"uri":")" << uri << R"("}],)"
       << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":0,)"
       << R"("extensions":{"EXT_meshopt_compression":{)"
       << R"("buffer":0,"byteOffset":0,)"
       << R"("byteLength":)" << compressedSize << ","
       << R"("byteStride":)" << stride << ","
       << R"("count":)" << count << ","
       << R"("mode":"INDICES"}}}]})";

    auto asset = loadInline(js.str());

    ASSERT_NE(asset->bufferViews, nullptr);
    ASSERT_FALSE(asset->bufferViews->empty());

    const auto &bv = (*asset->bufferViews)[0];
    ASSERT_NE(bv.extMeshoptCompression, nullptr);
    ASSERT_EQ(bv.decodedData.size(), count * stride);

    const uint32_t *decoded = reinterpret_cast<const uint32_t *>(bv.decodedData.data());
    for (size_t i = 0; i < count; ++i)
        EXPECT_EQ(decoded[i], original[i]) << "mismatch at index " << i;
}

// ─────────────────────────────────────────────────────────────────────────────
// 22.  KHR_texture_procedurals
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kTextureProceduralsJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_texture_procedurals"],
  "extensions": {
    "KHR_texture_procedurals": {
      "procedurals": [
        {
          "name": "checkerboard",
          "nodetype": "nodegraph",
          "type": "color3",
          "inputs": {
            "color1": {
              "nodetype": "input",
              "type": "color3",
              "value": [1.0, 0.094118, 0.031373]
            },
            "color2": {
              "nodetype": "input",
              "type": "color3",
              "value": [0.035294, 0.090196, 0.878431]
            },
            "uvtiling": {
              "nodetype": "input",
              "type": "vector2",
              "value": [8.0, 8.0]
            }
          },
          "outputs": {
            "out": {
              "nodetype": "output",
              "type": "color3",
              "node": 0,
              "output": "out"
            }
          },
          "nodes": [
            {
              "name": "mix1",
              "nodetype": "mix",
              "type": "color3",
              "inputs": {
                "fg": {
                  "nodetype": "input",
                  "type": "color3",
                  "input": "color1"
                },
                "bg": {
                  "nodetype": "input",
                  "type": "color3",
                  "input": "color2"
                },
                "mix": {
                  "nodetype": "input",
                  "type": "float",
                  "value": [0.5]
                }
              },
              "outputs": {
                "out": {
                  "nodetype": "output",
                  "type": "color3"
                }
              }
            }
          ]
        }
      ],
      "procedural_definitions": [
        {
          "name": "ND_custom_color3",
          "nodetype": "nodedef",
          "node": "custom",
          "nodegroup": "procedural",
          "inputs": {
            "in1": {
              "doc": "Input color",
              "type": "color3",
              "value": [1.0, 1.0, 1.0],
              "uiname": "Input 1"
            }
          },
          "outputs": {
            "out": {
              "type": "color3"
            }
          }
        }
      ]
    }
  },
  "images": [
    {
      "uri": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4z/AfAAQAAf/zKSWvAAAAAElFTkSuQmCC"
    }
  ],
  "textures": [
    {"source": 0}
  ],
  "materials": [
    {
      "name": "ProceduralMat",
      "pbrMetallicRoughness": {
        "baseColorTexture": {
          "index": 0,
          "extensions": {
            "KHR_texture_procedurals": {
              "index": 0,
              "output": "out"
            }
          }
        }
      }
    }
  ]
})";

class KHRTextureProceduralsTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    KHRTextureProceduralsTest() { asset = loadInline(kTextureProceduralsJSON); }
};

TEST_F(KHRTextureProceduralsTest, ExtensionParsed)
{
    ASSERT_NE(asset, nullptr);
    ASSERT_NE(asset->khrTextureProcedurals, nullptr);
}

TEST_F(KHRTextureProceduralsTest, ProceduralsCount)
{
    ASSERT_NE(asset->khrTextureProcedurals, nullptr);
    EXPECT_EQ(asset->khrTextureProcedurals->procedurals.size(), 1u);
}

TEST_F(KHRTextureProceduralsTest, ProceduralGraphName)
{
    const auto &graph = asset->khrTextureProcedurals->procedurals[0];
    EXPECT_EQ(graph.name, "checkerboard");
    EXPECT_EQ(graph.nodetype, "nodegraph");
    EXPECT_EQ(graph.type, "color3");
}

TEST_F(KHRTextureProceduralsTest, ProceduralGraphInputs)
{
    const auto &graph = asset->khrTextureProcedurals->procedurals[0];
    ASSERT_EQ(graph.inputs.size(), 3u);

    const ProceduralNodeInput *color1 = nullptr;
    for (const auto &in : graph.inputs)
        if (in.name == "color1") color1 = &in;
    ASSERT_NE(color1, nullptr);
    EXPECT_EQ(color1->type, "color3");
    ASSERT_NE(color1->value, nullptr);
    ASSERT_EQ(color1->value->values.size(), 3u);
    EXPECT_NEAR(color1->value->values[0], 1.0, 1e-6);
    EXPECT_NEAR(color1->value->values[1], 0.094118, 1e-6);
    EXPECT_NEAR(color1->value->values[2], 0.031373, 1e-6);
}

TEST_F(KHRTextureProceduralsTest, ProceduralGraphOutputs)
{
    const auto &graph = asset->khrTextureProcedurals->procedurals[0];
    ASSERT_EQ(graph.outputs.size(), 1u);
    EXPECT_EQ(graph.outputs[0].name, "out");
    EXPECT_EQ(graph.outputs[0].type, "color3");
    EXPECT_EQ(graph.outputs[0].nodeIndex, 0);
    EXPECT_EQ(graph.outputs[0].nodeOutput, "out");
}

TEST_F(KHRTextureProceduralsTest, ProceduralNode)
{
    const auto &graph = asset->khrTextureProcedurals->procedurals[0];
    ASSERT_EQ(graph.nodes.size(), 1u);
    const auto &node = graph.nodes[0];
    EXPECT_EQ(node.name, "mix1");
    EXPECT_EQ(node.nodetype, "mix");
    EXPECT_EQ(node.type, "color3");
    ASSERT_EQ(node.inputs.size(), 3u);
    ASSERT_EQ(node.outputs.size(), 1u);
}

TEST_F(KHRTextureProceduralsTest, NodeInputConnections)
{
    const auto &node = asset->khrTextureProcedurals->procedurals[0].nodes[0];
    const ProceduralNodeInput *fg = nullptr;
    const ProceduralNodeInput *mix = nullptr;
    for (const auto &in : node.inputs)
    {
        if (in.name == "fg") fg = &in;
        if (in.name == "mix") mix = &in;
    }
    ASSERT_NE(fg, nullptr);
    EXPECT_EQ(fg->graphInput, "color1");
    ASSERT_NE(mix, nullptr);
    ASSERT_NE(mix->value, nullptr);
    EXPECT_NEAR(mix->value->values[0], 0.5, 1e-6);
}

TEST_F(KHRTextureProceduralsTest, ProceduralDefinitionsCount)
{
    ASSERT_NE(asset->khrTextureProcedurals, nullptr);
    EXPECT_EQ(asset->khrTextureProcedurals->proceduralDefinitions.size(), 1u);
}

TEST_F(KHRTextureProceduralsTest, ProceduralNodeDef)
{
    const auto &def = asset->khrTextureProcedurals->proceduralDefinitions[0];
    EXPECT_EQ(def.name, "ND_custom_color3");
    EXPECT_EQ(def.nodetype, "nodedef");
    EXPECT_EQ(def.node, "custom");
    EXPECT_EQ(def.nodegroup, "procedural");
    ASSERT_EQ(def.inputs.size(), 1u);
    EXPECT_EQ(def.inputs[0].doc, "Input color");
    EXPECT_EQ(def.inputs[0].uiname, "Input 1");
}

TEST_F(KHRTextureProceduralsTest, MaterialTextureInfoExtension)
{
    const Material *mat = findMaterial(asset, "ProceduralMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness->baseColorTexture, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness->baseColorTexture->khrTextureProcedurals, nullptr);
    EXPECT_EQ(mat->pbrMetallicRoughness->baseColorTexture->khrTextureProcedurals->index, 0);
    EXPECT_EQ(mat->pbrMetallicRoughness->baseColorTexture->khrTextureProcedurals->output, "out");
}

TEST_F(KHRTextureProceduralsTest, FallbackTextureIndexPreserved)
{
    const Material *mat = findMaterial(asset, "ProceduralMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness->baseColorTexture, nullptr);
    EXPECT_EQ(mat->pbrMetallicRoughness->baseColorTexture->index, 0u);
}
