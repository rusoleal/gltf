/**
 * glTF 2.1 specific tests.
 *
 * These tests exercise the new core features introduced in the glTF 2.1
 * specification (announced June 2026). All tests use self-contained inline
 * JSON so no external files are needed.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <future>
#include <fstream>
#include <filesystem>
#include <limits>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    void writeU32(std::vector<uint8_t> &out, uint32_t value)
    {
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void writeU64(std::vector<uint8_t> &out, uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
        {
            out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    uint64_t align64(uint64_t value)
    {
        if (value % 8 == 0) return value;
        return value + (8 - (value % 8));
    }

    std::vector<uint8_t> makeGLBv3(const std::string &json, const std::vector<uint8_t> &bin = {})
    {
        uint64_t jsonLen = json.size();
        uint64_t jsonPadded = align64(jsonLen);
        uint64_t binLen = bin.size();
        uint64_t binPadded = align64(binLen);
        uint64_t total = 16 + 16 + jsonPadded + (bin.empty() ? 0 : 16 + binPadded);

        std::vector<uint8_t> out;
        out.reserve(static_cast<size_t>(total));

        // Header: magic + version + total length
        writeU32(out, 0x46546C67);
        writeU32(out, 3);
        writeU64(out, total);

        // JSON chunk
        writeU64(out, jsonLen);
        writeU32(out, 0x4E4F534A); // JSON
        writeU32(out, 0);          // encoding
        out.insert(out.end(), json.begin(), json.end());
        out.resize(static_cast<size_t>(16 + 16 + jsonPadded), 0);

        // Optional BIN chunk
        if (!bin.empty())
        {
            writeU64(out, binLen);
            writeU32(out, 0x004E4942); // BIN
            writeU32(out, 0);          // encoding
            out.insert(out.end(), bin.begin(), bin.end());
            out.resize(static_cast<size_t>(total), 0);
        }

        return out;
    }
    std::shared_ptr<GLTF> parse21(const std::string &json)
    {
        return GLTF::loadGLTF(json, [](const std::string &) {
            return std::async(std::launch::deferred, []() {
                return std::vector<uint8_t>{};
            });
        });
    }

    std::shared_ptr<GLTF> loadFixture(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.good())
        {
            return nullptr;
        }
        std::string json((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        return GLTF::loadGLTF(json, [](const std::string &uri) {
            return std::async(std::launch::deferred, [&uri]() {
                std::ifstream f(uri, std::ios::binary);
                if (!f.good()) return std::vector<uint8_t>{};
                return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                             std::istreambuf_iterator<char>());
            });
        });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Version acceptance
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, Version21Accepted)
{
    auto asset = parse21(R"({"asset": {"version": "2.1"}})");
    ASSERT_NE(asset, nullptr);
}

TEST(GLTF21Test, Version20StillAccepted)
{
    auto asset = parse21(R"({"asset": {"version": "2.0"}})");
    ASSERT_NE(asset, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. KHR_node_visibility promoted to core
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, NodeVisibleDefaultsToTrue)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [{}]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_TRUE((*asset->nodes)[0].visible);
}

TEST(GLTF21Test, NodeVisibleFalseParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"visible": false}]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_FALSE((*asset->nodes)[0].visible);
}

TEST(GLTF21Test, NodeVisibleTrueParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"visible": true}]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_TRUE((*asset->nodes)[0].visible);
}

TEST(GLTF21Test, NodeVisibilityMixed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [
            {"name": "VisibleNode"},
            {"name": "HiddenNode", "visible": false}
        ]
    })");
    ASSERT_EQ(asset->nodes->size(), 2u);
    EXPECT_TRUE((*asset->nodes)[0].visible);
    EXPECT_FALSE((*asset->nodes)[1].visible);
}

TEST(GLTF21Test, NodeVisibilityExtensionForm)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.0"},
        "extensionsUsed": ["KHR_node_visibility"],
        "nodes": [
            {"extensions": {"KHR_node_visibility": {"visible": false}}}
        ]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_FALSE((*asset->nodes)[0].visible);
}

TEST(GLTF21Test, NodeVisibilityCoreTakesPrecedenceOverExtension)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [
            {
                "visible": true,
                "extensions": {"KHR_node_visibility": {"visible": false}}
            }
        ]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_TRUE((*asset->nodes)[0].visible);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Accessor component type definitions
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, AccessorComponentTypeSignedInt)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "accessors": [{"componentType": 5124, "count": 1, "type": "SCALAR"}]
    })");
    ASSERT_EQ(asset->accessors->size(), 1u);
    EXPECT_EQ((*asset->accessors)[0].componentType, ComponentType::ctSignedInt);
}

TEST(GLTF21Test, AccessorComponentTypeDouble)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "accessors": [{"componentType": 5130, "count": 1, "type": "SCALAR"}]
    })");
    ASSERT_EQ(asset->accessors->size(), 1u);
    EXPECT_EQ((*asset->accessors)[0].componentType, ComponentType::ctDouble);
}

TEST(GLTF21Test, AccessorComponentTypeHalfFloat)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "accessors": [{"componentType": 5131, "count": 1, "type": "SCALAR"}]
    })");
    ASSERT_EQ(asset->accessors->size(), 1u);
    EXPECT_EQ((*asset->accessors)[0].componentType, ComponentType::ctHalfFloat);
}

TEST(GLTF21Test, AccessorComponentTypeSignedInt64)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "accessors": [{"componentType": 5134, "count": 1, "type": "SCALAR"}]
    })");
    ASSERT_EQ(asset->accessors->size(), 1u);
    EXPECT_EQ((*asset->accessors)[0].componentType, ComponentType::ctSignedInt64);
}

TEST(GLTF21Test, AccessorComponentTypeUnsignedInt64)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "accessors": [{"componentType": 5135, "count": 1, "type": "SCALAR"}]
    })");
    ASSERT_EQ(asset->accessors->size(), 1u);
    EXPECT_EQ((*asset->accessors)[0].componentType, ComponentType::ctUnsignedInt64);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Non-sequential attributes
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, NonSequentialTexCoord)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "meshes": [{
            "primitives": [{
                "attributes": {"POSITION": 0, "TEXCOORD_1": 1}
            }]
        }],
        "accessors": [
            {"componentType": 5126, "count": 1, "type": "VEC3"},
            {"componentType": 5126, "count": 1, "type": "VEC2"}
        ]
    })");
    ASSERT_EQ(asset->meshes->size(), 1u);
    const auto &attrs = asset->meshes->front().primitives.front().attributes;
    EXPECT_EQ(attrs.at("POSITION"), 0u);
    EXPECT_EQ(attrs.at("TEXCOORD_1"), 1u);
    EXPECT_EQ(attrs.count("TEXCOORD_0"), 0u);
}

TEST(GLTF21Test, NonSequentialColor)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "meshes": [{
            "primitives": [{
                "attributes": {"POSITION": 0, "COLOR_2": 1}
            }]
        }],
        "accessors": [
            {"componentType": 5126, "count": 1, "type": "VEC3"},
            {"componentType": 5121, "count": 1, "type": "VEC4"}
        ]
    })");
    ASSERT_EQ(asset->meshes->size(), 1u);
    const auto &attrs = asset->meshes->front().primitives.front().attributes;
    EXPECT_EQ(attrs.at("POSITION"), 0u);
    EXPECT_EQ(attrs.at("COLOR_2"), 1u);
    EXPECT_EQ(attrs.count("COLOR_0"), 0u);
    EXPECT_EQ(attrs.count("COLOR_1"), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Unique IDs
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, UidParsedOnNode)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"uid": "node-01"}]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_EQ((*asset->nodes)[0].uid, "node-01");
}

TEST(GLTF21Test, UidParsedOnMultipleRootObjects)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"uid": "node-01"}],
        "meshes": [{"uid": "mesh-01", "primitives": []}],
        "materials": [{"uid": "mat-01"}]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    ASSERT_EQ(asset->meshes->size(), 1u);
    ASSERT_EQ(asset->materials->size(), 1u);
    EXPECT_EQ((*asset->nodes)[0].uid, "node-01");
    EXPECT_EQ((*asset->meshes)[0].uid, "mesh-01");
    EXPECT_EQ((*asset->materials)[0].uid, "mat-01");
}

TEST(GLTF21Test, DuplicateUidThrows)
{
    EXPECT_THROW(parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [
            {"uid": "same-id"},
            {"uid": "same-id"}
        ]
    })"), std::invalid_argument);
}

TEST(GLTF21Test, UidCollidingWithNameThrows)
{
    EXPECT_THROW(parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [
            {"name": "reserved"},
            {"uid": "reserved"}
        ]
    })"), std::invalid_argument);
}

TEST(GLTF21Test, NameCollidingWithEarlierUidThrows)
{
    EXPECT_THROW(parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [
            {"uid": "reserved"},
            {"name": "reserved"}
        ]
    })"), std::invalid_argument);
}

TEST(GLTF21Test, MissingUidIsEmpty)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"name": "PlainNode"}]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_EQ((*asset->nodes)[0].uid, "");
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Fixture integration tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21FixtureTest, LoadVersion21)
{
    auto asset = loadFixture("gltf21/version21.gltf");
    if (asset == nullptr)
    {
        GTEST_SKIP() << "Fixture gltf21/version21.gltf not found";
    }
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_EQ((*asset->nodes)[0].name, "Root");
    EXPECT_TRUE((*asset->nodes)[0].visible);
}

TEST(GLTF21FixtureTest, LoadUidTest)
{
    auto asset = loadFixture("gltf21/uid_test.gltf");
    if (asset == nullptr)
    {
        GTEST_SKIP() << "Fixture gltf21/uid_test.gltf not found";
    }
    ASSERT_EQ(asset->scenes->size(), 1u);
    EXPECT_EQ((*asset->scenes)[0].uid, "scene-main");
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_EQ((*asset->nodes)[0].uid, "node-root");
    ASSERT_EQ(asset->meshes->size(), 1u);
    EXPECT_EQ((*asset->meshes)[0].uid, "mesh-triangle");
    ASSERT_EQ(asset->accessors->size(), 1u);
    EXPECT_EQ((*asset->accessors)[0].uid, "accessor-pos");
    ASSERT_EQ(asset->bufferViews->size(), 1u);
    EXPECT_EQ((*asset->bufferViews)[0].uid, "bufferView-pos");
    ASSERT_EQ(asset->buffers->size(), 1u);
    EXPECT_EQ((*asset->buffers)[0].uid, "buffer-main");
}

TEST(GLTF21FixtureTest, LoadNonSequential)
{
    auto asset = loadFixture("gltf21/nonsequential.gltf");
    if (asset == nullptr)
    {
        GTEST_SKIP() << "Fixture gltf21/nonsequential.gltf not found";
    }
    ASSERT_EQ(asset->meshes->size(), 1u);
    const auto &attrs = asset->meshes->front().primitives.front().attributes;
    EXPECT_EQ(attrs.at("POSITION"), 0u);
    EXPECT_EQ(attrs.at("TEXCOORD_2"), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Unified File References (files array)
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, FilesArrayParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "files": [
            {"mimeType": "model/gltf-binary", "uri": "sub_model.glb"},
            {"name": "embedded", "mimeType": "audio/mpeg", "bufferView": 0}
        ],
        "bufferViews": [{"buffer": 0, "byteLength": 0}],
        "buffers": [{"byteLength": 0}]
    })");
    ASSERT_NE(asset->files, nullptr);
    ASSERT_EQ(asset->files->size(), 2u);
    EXPECT_EQ((*asset->files)[0].mimeType, "model/gltf-binary");
    EXPECT_EQ((*asset->files)[0].uri, "sub_model.glb");
    EXPECT_EQ((*asset->files)[1].name, "embedded");
    EXPECT_EQ((*asset->files)[1].bufferView, 0);
    EXPECT_EQ((*asset->files)[1].uri, "");
}

TEST(GLTF21Test, FileMissingMimeTypeThrows)
{
    EXPECT_THROW(parse21(R"({
        "asset": {"version": "2.1"},
        "files": [{"uri": "foo.bin"}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. External Assets
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, ExternalAssetParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "files": [{"mimeType": "model/gltf-binary", "uri": "sub_model.glb"}],
        "externalAssets": [{"name": "sub_model_glb", "file": 0}],
        "nodes": [{"externalAsset": 0}]
    })");
    ASSERT_NE(asset->externalAssets, nullptr);
    ASSERT_EQ(asset->externalAssets->size(), 1u);
    EXPECT_EQ((*asset->externalAssets)[0].name, "sub_model_glb");
    EXPECT_EQ((*asset->externalAssets)[0].file, 0);
    ASSERT_EQ(asset->nodes->size(), 1u);
    EXPECT_EQ((*asset->nodes)[0].externalAsset, 0);
}

TEST(GLTF21Test, ExternalAssetMissingFileThrows)
{
    EXPECT_THROW(parse21(R"({
        "asset": {"version": "2.1"},
        "externalAssets": [{"name": "bad"}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. Shapes
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, ShapesArrayParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "shapes": [
            {"type": "box", "box": {"size": [2.5, 3.0, 2.0]}},
            {"type": "sphere", "sphere": {"radius": 1.5}},
            {"type": "capsule", "capsule": {"height": 2.0, "radiusTop": 0.5, "radiusBottom": 0.6}},
            {"type": "cylinder", "cylinder": {"height": 1.0, "radiusTop": 0.4, "radiusBottom": 0.7}},
            {"type": "plane", "plane": {"doubleSided": true, "sizeX": 4.0, "sizeZ": 2.0}}
        ]
    })");
    ASSERT_NE(asset->shapes, nullptr);
    ASSERT_EQ(asset->shapes->size(), 5u);

    auto *box = dynamic_cast<BoxShape *>(asset->shapes->at(0).get());
    ASSERT_NE(box, nullptr);
    EXPECT_DOUBLE_EQ(box->size.x(), 2.5);
    EXPECT_DOUBLE_EQ(box->size.y(), 3.0);
    EXPECT_DOUBLE_EQ(box->size.z(), 2.0);

    auto *sphere = dynamic_cast<SphereShape *>(asset->shapes->at(1).get());
    ASSERT_NE(sphere, nullptr);
    EXPECT_DOUBLE_EQ(sphere->radius, 1.5);

    auto *capsule = dynamic_cast<CapsuleShape *>(asset->shapes->at(2).get());
    ASSERT_NE(capsule, nullptr);
    EXPECT_DOUBLE_EQ(capsule->height, 2.0);
    EXPECT_DOUBLE_EQ(capsule->radiusTop, 0.5);
    EXPECT_DOUBLE_EQ(capsule->radiusBottom, 0.6);

    auto *cylinder = dynamic_cast<CylinderShape *>(asset->shapes->at(3).get());
    ASSERT_NE(cylinder, nullptr);
    EXPECT_DOUBLE_EQ(cylinder->height, 1.0);

    auto *plane = dynamic_cast<PlaneShape *>(asset->shapes->at(4).get());
    ASSERT_NE(plane, nullptr);
    EXPECT_TRUE(plane->doubleSided);
    ASSERT_TRUE(plane->sizeX.has_value());
    EXPECT_DOUBLE_EQ(plane->sizeX.value(), 4.0);
    ASSERT_TRUE(plane->sizeZ.has_value());
    EXPECT_DOUBLE_EQ(plane->sizeZ.value(), 2.0);
}

TEST(GLTF21Test, UnknownShapeTypeThrows)
{
    EXPECT_THROW(parse21(R"({
        "asset": {"version": "2.1"},
        "shapes": [{"type": "torus"}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 10. Bounding Volumes
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, BoundingVolumeParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "shapes": [{"type": "box", "box": {"size": [1, 1, 1]}}],
        "nodes": [{"boundingVolume": {"shape": 0}}]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    ASSERT_NE((*asset->nodes)[0].boundingVolume, nullptr);
    EXPECT_EQ((*asset->nodes)[0].boundingVolume->shape, 0);
}

TEST(GLTF21Test, BoundingVolumeWithMatrixParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"},
        "shapes": [{"type": "sphere", "sphere": {"radius": 1}}],
        "nodes": [{
            "boundingVolume": {
                "shape": 0,
                "matrix": [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1]
            }
        }]
    })");
    ASSERT_EQ(asset->nodes->size(), 1u);
    ASSERT_NE((*asset->nodes)[0].boundingVolume, nullptr);
    EXPECT_EQ((*asset->nodes)[0].boundingVolume->shape, 0);
    // After column-major glTF input is transposed to row-major storage,
    // the translation occupies the last column: data[3], data[7], data[11].
    EXPECT_DOUBLE_EQ((*asset->nodes)[0].boundingVolume->matrix.data[3], 1.0);
    EXPECT_DOUBLE_EQ((*asset->nodes)[0].boundingVolume->matrix.data[7], 2.0);
    EXPECT_DOUBLE_EQ((*asset->nodes)[0].boundingVolume->matrix.data[11], 3.0);
}

TEST(GLTF21Test, BoundingVolumeMissingShapeThrows)
{
    EXPECT_THROW(parse21(R"({
        "asset": {"version": "2.1"},
        "nodes": [{"boundingVolume": {}}]
    })"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// 11. Thumbnails
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, ThumbnailParsed)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1", "thumbnail": 0},
        "images": [{"mimeType": "image/png", "uri": "thumbnail.png"}]
    })");
    EXPECT_EQ(asset->thumbnail, 0);
}

TEST(GLTF21Test, NoThumbnailDefaultsToMinusOne)
{
    auto asset = parse21(R"({
        "asset": {"version": "2.1"}
    })");
    EXPECT_EQ(asset->thumbnail, -1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 12. Official Khronos 2.1 sample assets (optional — skips when not downloaded)
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21OfficialTest, LoadAllDownloadedOfficialSamples)
{
    std::filesystem::path dir("gltf21_official");
    if (!std::filesystem::is_directory(dir))
    {
        GTEST_SKIP() << "Official glTF 2.1 sample directory not present; "
                        "run launch_test.sh with GLTF21_SAMPLE_REPO set to enable downloads.";
    }

    bool found = false;
    for (const auto &entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension();
        if (ext != ".gltf") continue;

        found = true;
        auto path = entry.path().string();
        auto asset = loadFixture(path);
        ASSERT_NE(asset, nullptr) << "Failed to parse official sample: " << path;
    }

    if (!found)
    {
        GTEST_SKIP() << "No official glTF 2.1 .gltf samples found in gltf21_official/";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 13. 64-bit GLB (version 3)
// ─────────────────────────────────────────────────────────────────────────────

TEST(GLTF21Test, GLBv3LoadsMinimalJSONOnly)
{
    std::string json = R"({"asset":{"version":"2.1"}})";
    auto glb = makeGLBv3(json);
    auto asset = GLTF::loadGLB(glb.data(), glb.size());
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->nodes->size(), 0u);
}

TEST(GLTF21Test, GLBv3LoadsWithBinaryChunk)
{
    std::string json = R"({"asset":{"version":"2.1"},"buffers":[{"byteLength":4}]})";
    std::vector<uint8_t> bin = {0xDE, 0xAD, 0xBE, 0xEF};
    auto glb = makeGLBv3(json, bin);
    auto asset = GLTF::loadGLB(glb.data(), glb.size());
    ASSERT_NE(asset, nullptr);
    ASSERT_EQ(asset->buffers->size(), 1u);
    ASSERT_EQ((*asset->buffers)[0].data.size(), 4u);
    EXPECT_EQ((*asset->buffers)[0].data[0], 0xDE);
    EXPECT_EQ((*asset->buffers)[0].data[1], 0xAD);
    EXPECT_EQ((*asset->buffers)[0].data[2], 0xBE);
    EXPECT_EQ((*asset->buffers)[0].data[3], 0xEF);
}

TEST(GLTF21Test, GLBv4Rejected)
{
    std::vector<uint8_t> glb;
    writeU32(glb, 0x46546C67);
    writeU32(glb, 4);
    writeU64(glb, 16);
    EXPECT_THROW(GLTF::loadGLB(glb.data(), glb.size()), std::invalid_argument);
}
