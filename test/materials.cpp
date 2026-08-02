/**
 * Core PBR material numeric-precision regression tests.
 *
 * nlohmann::json::value<ValueType>(key, default) deduces ValueType from the
 * *default argument's* literal type, not from the variable it is assigned
 * to. Several call sites in gltf.cpp used to pass a bare integer literal
 * (e.g. `1`) as the default for fields that are GLTF_REAL_NUMBER_TYPE
 * (float/double). That made the parser call get<int>() on the underlying
 * JSON number, silently truncating any fractional value toward zero (e.g.
 * a metallicFactor of 0.35 would be read back as 0) before it was ever
 * converted to GLTF_REAL_NUMBER_TYPE.
 *
 * All tests use self-contained inline JSON so no network access is needed.
 * findMaterial() is defined in main.cpp and shared here via the unity build.
 */

#include <future>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

namespace
{
    std::shared_ptr<GLTF> parseMat(const std::string &json)
    {
        return GLTF::loadGLTF(json, [](const std::string &) {
            return std::async(std::launch::deferred, []() {
                return std::vector<uint8_t>{};
            });
        });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Fractional factors must round-trip exactly, not truncate toward zero.
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kFractionalPBRJSON = R"({
  "asset": {"version": "2.0"},
  "materials": [
    {
      "name": "FractionalMat",
      "pbrMetallicRoughness": {
        "metallicFactor": 0.35,
        "roughnessFactor": 0.72
      },
      "normalTexture":    {"index": 0, "scale": 1.75},
      "occlusionTexture": {"index": 0, "strength": 0.65}
    }
  ]
})";

class PBRFractionalFactorsTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    PBRFractionalFactorsTest() { asset = parseMat(kFractionalPBRJSON); }
};

TEST_F(PBRFractionalFactorsTest, MetallicFactorNotTruncated)
{
    const Material *mat = findMaterial(asset, "FractionalMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
    EXPECT_NEAR(mat->pbrMetallicRoughness->metallicFactor, 0.35, 1e-6);
}

TEST_F(PBRFractionalFactorsTest, RoughnessFactorNotTruncated)
{
    const Material *mat = findMaterial(asset, "FractionalMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
    EXPECT_NEAR(mat->pbrMetallicRoughness->roughnessFactor, 0.72, 1e-6);
}

TEST_F(PBRFractionalFactorsTest, NormalTextureScaleNotTruncated)
{
    const Material *mat = findMaterial(asset, "FractionalMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->normalTexture, nullptr);
    EXPECT_NEAR(mat->normalTexture->scale, 1.75, 1e-6);
}

TEST_F(PBRFractionalFactorsTest, OcclusionTextureStrengthNotTruncated)
{
    const Material *mat = findMaterial(asset, "FractionalMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->occlusionTexture, nullptr);
    EXPECT_NEAR(mat->occlusionTexture->strength, 0.65, 1e-6);
}

// ─────────────────────────────────────────────────────────────────────────────
// Omitted fields must default to exactly 1.0 (the spec default), including
// through the same value()-with-default code path exercised above.
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kDefaultFactorsJSON = R"({
  "asset": {"version": "2.0"},
  "materials": [
    {
      "name": "BareMat",
      "pbrMetallicRoughness": {},
      "normalTexture": {"index": 0},
      "occlusionTexture": {"index": 0}
    }
  ]
})";

class PBRDefaultFactorsTest : public testing::Test {
protected:
    std::shared_ptr<GLTF> asset;
    PBRDefaultFactorsTest() { asset = parseMat(kDefaultFactorsJSON); }
};

TEST_F(PBRDefaultFactorsTest, MetallicAndRoughnessDefaultToOne)
{
    const Material *mat = findMaterial(asset, "BareMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
    EXPECT_NEAR(mat->pbrMetallicRoughness->metallicFactor, 1.0, 1e-9);
    EXPECT_NEAR(mat->pbrMetallicRoughness->roughnessFactor, 1.0, 1e-9);
}

TEST_F(PBRDefaultFactorsTest, NormalScaleDefaultsToOne)
{
    const Material *mat = findMaterial(asset, "BareMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->normalTexture, nullptr);
    EXPECT_NEAR(mat->normalTexture->scale, 1.0, 1e-9);
}

TEST_F(PBRDefaultFactorsTest, OcclusionStrengthDefaultsToOne)
{
    const Material *mat = findMaterial(asset, "BareMat");
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(mat->occlusionTexture, nullptr);
    EXPECT_NEAR(mat->occlusionTexture->strength, 1.0, 1e-9);
}

// ─────────────────────────────────────────────────────────────────────────────
// getRuntimeInfo() must mark an image as used when a texture references it
// only through an extension (EXT_texture_webp / KHR_texture_basisu /
// EXT_texture_astc) with no base "source" field. "source" is optional
// precisely because these extensions are meant to fully replace it, so
// exporters commonly omit it — updateRuntimeInfoWithMaterial() used to check
// only texture.source, silently leaving every such image's runtimeInfo slot
// false. Consumers (like campello_renderer) skip uploading any image whose
// slot reads false, so every material using an extension-only texture would
// render with default placeholder textures (flat white base color, fully
// metallic) instead of its real data.
// ─────────────────────────────────────────────────────────────────────────────

static const std::string kWebpOnlyTextureJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["EXT_texture_webp"],
  "buffers": [
    {"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAA", "byteLength": 12}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 12}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 1, "type": "VEC3"}
  ],
  "images": [
    {"uri": "placeholder.webp"}
  ],
  "textures": [
    {"extensions": {"EXT_texture_webp": {"source": 0}}}
  ],
  "materials": [
    {
      "name": "WebpOnlyMat",
      "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}
    }
  ],
  "meshes": [
    {"primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}
  ],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";

TEST(RuntimeInfoExtensionOnlyTextureTest, WebpOnlyImageMarkedUsed)
{
    auto asset = parseMat(kWebpOnlyTextureJSON);
    ASSERT_NE(asset, nullptr);
    auto info = asset->getRuntimeInfo(0);
    ASSERT_NE(info, nullptr);
    ASSERT_EQ(info->images.size(), 1u);
    EXPECT_TRUE(info->images[0])
        << "image referenced only via EXT_texture_webp must be marked used";
}

static const std::string kBasisuOnlyTextureJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_texture_basisu"],
  "buffers": [
    {"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAA", "byteLength": 12}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 12}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 1, "type": "VEC3"}
  ],
  "images": [
    {"uri": "compressed.ktx2"}
  ],
  "textures": [
    {"extensions": {"KHR_texture_basisu": {"source": 0}}}
  ],
  "materials": [
    {
      "name": "BasisuOnlyMat",
      "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}
    }
  ],
  "meshes": [
    {"primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}
  ],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";

TEST(RuntimeInfoExtensionOnlyTextureTest, BasisuOnlyImageMarkedUsed)
{
    auto asset = parseMat(kBasisuOnlyTextureJSON);
    ASSERT_NE(asset, nullptr);
    auto info = asset->getRuntimeInfo(0);
    ASSERT_NE(info, nullptr);
    ASSERT_EQ(info->images.size(), 1u);
    EXPECT_TRUE(info->images[0])
        << "image referenced only via KHR_texture_basisu must be marked used";
}

static const std::string kAstcOnlyTextureJSON = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["EXT_texture_astc"],
  "buffers": [
    {"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAA", "byteLength": 12}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 12}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 1, "type": "VEC3"}
  ],
  "images": [
    {"uri": "compressed.astc.ktx2"}
  ],
  "textures": [
    {"extensions": {"EXT_texture_astc": {"source": 0}}}
  ],
  "materials": [
    {
      "name": "AstcOnlyMat",
      "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}
    }
  ],
  "meshes": [
    {"primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}
  ],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";

TEST(RuntimeInfoExtensionOnlyTextureTest, AstcOnlyImageMarkedUsed)
{
    auto asset = parseMat(kAstcOnlyTextureJSON);
    ASSERT_NE(asset, nullptr);
    auto info = asset->getRuntimeInfo(0);
    ASSERT_NE(info, nullptr);
    ASSERT_EQ(info->images.size(), 1u);
    EXPECT_TRUE(info->images[0])
        << "image referenced only via EXT_texture_astc must be marked used";
}
