#include <fstream>
#include <streambuf>
#include <cstdio>
#include <algorithm>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

std::string loadFileAsString(const std::string &filename) {
    std::ifstream t(filename, std::ios::binary);
    std::string str;

    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
                std::istreambuf_iterator<char>());
    return str;
}

// Helper: find a node by name, returns nullptr if not found
static const Node* findNode(const std::shared_ptr<GLTF> &asset, const std::string &name) {
    for (const auto &node : *asset->nodes) {
        if (node.name == name) return &node;
    }
    return nullptr;
}

// Helper: find a material by name, returns nullptr if not found
static const Material* findMaterial(const std::shared_ptr<GLTF> &asset, const std::string &name) {
    for (const auto &mat : *asset->materials) {
        if (mat.name == name) return &mat;
    }
    return nullptr;
}

// Helper: find a mesh by name, returns nullptr if not found
const Mesh* findMesh(const std::shared_ptr<GLTF> &asset, const std::string &name) {
    for (const auto &mesh : *asset->meshes) {
        if (mesh.name == name) return &mesh;
    }
    return nullptr;
}

// ─── Fixture base ────────────────────────────────────────────────────────────

class AlphaBlendModeTest: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTest() {
    auto result = loadFileAsString("AlphaBlendModeTest.gltf");
    asset = GLTF::loadGLTF(result);
  }
};

// Minimal self-contained embedded glTF (no external files needed).
// Buffer: 12 zero bytes (one VEC3 at origin), base64-encoded.
// Image:  1×1 white PNG, base64-encoded.
static const std::string kEmbeddedGLTF = R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"name": "EmbeddedScene", "nodes": [0]}],
  "nodes":  [{"name": "EmbeddedNode",  "mesh": 0}],
  "meshes": [{"name": "EmbeddedMesh",  "primitives": [{"attributes": {"POSITION": 0}, "mode": 4}]}],
  "accessors": [{"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 1, "type": "VEC3"}],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 12}],
  "buffers": [{"byteLength": 12, "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAA"}],
  "images": [{"uri": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="}]
})";

class AlphaBlendModeTestEmbedded: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTestEmbedded() {
    asset = GLTF::loadGLTF(kEmbeddedGLTF);
  }
};

class AlphaBlendModeTestGLB: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTestGLB() {
    auto result = loadFileAsString("AlphaBlendModeTest.glb");
    asset = GLTF::loadGLB((uint8_t *)result.c_str(), result.size());
  }
};

// ─── .gltf (external buffers/images) ────────────────────────────────────────

TEST_F(AlphaBlendModeTest, Counts) {
  EXPECT_EQ(asset->buffers->size(), 1);
  EXPECT_EQ(asset->bufferViews->size(), 45);
  EXPECT_EQ(asset->accessors->size(), 45);
  EXPECT_EQ(asset->cameras->size(), 0);
  EXPECT_EQ(asset->images->size(), 4);
  EXPECT_EQ(asset->textures->size(), 4);
  EXPECT_EQ(asset->scenes->size(), 1);
  EXPECT_EQ(asset->nodes->size(), 9);
  EXPECT_EQ(asset->samplers->size(), 1);
  EXPECT_EQ(asset->meshes->size(), 9);
  EXPECT_EQ(asset->materials->size(), 6);
  EXPECT_EQ(asset->animations->size(), 0);
  EXPECT_EQ(asset->skins->size(), 0);
  EXPECT_EQ(asset->khrLightsPunctual->size(), 0);
}

TEST_F(AlphaBlendModeTest, Buffer_Properties) {
  const auto &buf = (*asset->buffers)[0];
  EXPECT_EQ(buf.byteLength, 6776);
  EXPECT_EQ(buf.uri, "AlphaBlendModeTest.bin");
  // External buffer: data array should be empty until loaded externally
  EXPECT_EQ(buf.data.size(), 0);
}

TEST_F(AlphaBlendModeTest, Images_External) {
  for (size_t i = 0; i < asset->images->size(); i++) {
    EXPECT_EQ((*asset->images)[i].data.size(), 0) << "image " << i << " should have no embedded data";
  }
}

TEST_F(AlphaBlendModeTest, Scene_Properties) {
  EXPECT_EQ(asset->scene, 0);
  const auto &scene = (*asset->scenes)[0];
  EXPECT_EQ(scene.name, "Scene");
  ASSERT_NE(scene.nodes, nullptr);
  EXPECT_EQ(scene.nodes->size(), 9);
}

TEST_F(AlphaBlendModeTest, Textures) {
  for (size_t i = 0; i < asset->textures->size(); i++) {
    const auto &tex = (*asset->textures)[i];
    // All textures reference sampler 0
    EXPECT_EQ(tex.sampler, 0) << "texture " << i;
    // Source index should match the texture index in this model
    EXPECT_EQ(tex.source, (int64_t)i) << "texture " << i;
    // No EXT_texture_webp in this model
    EXPECT_EQ(tex.ext_texture_webp, -1) << "texture " << i;
  }
}

TEST_F(AlphaBlendModeTest, Sampler_Defaults) {
  const auto &sampler = (*asset->samplers)[0];
  // The AlphaBlendModeTest uses an empty sampler object — defaults apply
  EXPECT_EQ(sampler.magFilter, FilterMode::fmUnknown);
  EXPECT_EQ(sampler.minFilter, FilterMode::fmUnknown);
  EXPECT_EQ(sampler.wrapS, WrapMode::repeat);
  EXPECT_EQ(sampler.wrapT, WrapMode::repeat);
}

// --- Materials ---------------------------------------------------------------

TEST_F(AlphaBlendModeTest, Material_Names) {
  std::vector<std::string> expectedNames = {
    "MatBed", "MatBlend", "MatCutoff25", "MatCutoff75", "MatCutoffDefault", "MatOpaque"
  };
  ASSERT_EQ(asset->materials->size(), expectedNames.size());
  for (const auto &name : expectedNames) {
    EXPECT_NE(findMaterial(asset, name), nullptr) << "missing material: " << name;
  }
}

TEST_F(AlphaBlendModeTest, Material_AlphaMode_Opaque) {
  const Material *mat = findMaterial(asset, "MatOpaque");
  ASSERT_NE(mat, nullptr);
  EXPECT_EQ(mat->alphaMode, AlphaMode::opaque);
  EXPECT_TRUE(mat->doubleSided);
}

TEST_F(AlphaBlendModeTest, Material_AlphaMode_Blend) {
  const Material *mat = findMaterial(asset, "MatBlend");
  ASSERT_NE(mat, nullptr);
  EXPECT_EQ(mat->alphaMode, AlphaMode::blend);
  EXPECT_TRUE(mat->doubleSided);
}

TEST_F(AlphaBlendModeTest, Material_AlphaMode_Mask_Cutoff25) {
  const Material *mat = findMaterial(asset, "MatCutoff25");
  ASSERT_NE(mat, nullptr);
  EXPECT_EQ(mat->alphaMode, AlphaMode::mask);
  EXPECT_NEAR(mat->alphaCutoff, 0.25, 1e-6);
  EXPECT_TRUE(mat->doubleSided);
}

TEST_F(AlphaBlendModeTest, Material_AlphaMode_Mask_Cutoff75) {
  const Material *mat = findMaterial(asset, "MatCutoff75");
  ASSERT_NE(mat, nullptr);
  EXPECT_EQ(mat->alphaMode, AlphaMode::mask);
  EXPECT_NEAR(mat->alphaCutoff, 0.75, 1e-6);
  EXPECT_TRUE(mat->doubleSided);
}

TEST_F(AlphaBlendModeTest, Material_AlphaMode_Mask_CutoffDefault) {
  const Material *mat = findMaterial(asset, "MatCutoffDefault");
  ASSERT_NE(mat, nullptr);
  EXPECT_EQ(mat->alphaMode, AlphaMode::mask);
  // When alphaCutoff is omitted, default is 0.5
  EXPECT_NEAR(mat->alphaCutoff, 0.5, 1e-6);
  EXPECT_TRUE(mat->doubleSided);
}

TEST_F(AlphaBlendModeTest, Material_MatBed_Properties) {
  const Material *mat = findMaterial(asset, "MatBed");
  ASSERT_NE(mat, nullptr);
  EXPECT_EQ(mat->alphaMode, AlphaMode::opaque);
  EXPECT_FALSE(mat->doubleSided);
  // MatBed has normal, occlusion, and metallic-roughness textures
  ASSERT_NE(mat->pbrMetallicRoughness, nullptr);
  ASSERT_NE(mat->normalTexture, nullptr);
  ASSERT_NE(mat->occlusionTexture, nullptr);
}

TEST_F(AlphaBlendModeTest, Material_BaseColorTexture) {
  // These materials reference the label texture (index 3) as base color
  for (const auto &name : std::vector<std::string>{"MatBlend", "MatCutoff25", "MatCutoff75", "MatCutoffDefault", "MatOpaque"}) {
    const Material *mat = findMaterial(asset, name);
    ASSERT_NE(mat, nullptr) << name;
    ASSERT_NE(mat->pbrMetallicRoughness, nullptr) << name;
    ASSERT_NE(mat->pbrMetallicRoughness->baseColorTexture, nullptr) << name;
    EXPECT_EQ(mat->pbrMetallicRoughness->baseColorTexture->index, 3u) << name;
  }
}

// --- Nodes -------------------------------------------------------------------

TEST_F(AlphaBlendModeTest, Node_Names) {
  std::vector<std::string> expectedNames = {
    "Bed", "DecalBlend", "DecalOpaque", "GreenArrows",
    "TestBlend", "TestCutoff25", "TestCutoff75", "TestCutoffDefault", "TestOpaque"
  };
  ASSERT_EQ(asset->nodes->size(), expectedNames.size());
  for (const auto &name : expectedNames) {
    EXPECT_NE(findNode(asset, name), nullptr) << "missing node: " << name;
  }
}

TEST_F(AlphaBlendModeTest, Node_TestOpaque_Translation) {
  const Node *node = findNode(asset, "TestOpaque");
  ASSERT_NE(node, nullptr);
  EXPECT_NEAR(node->translation.data[0], -3.0, 1e-5);
  EXPECT_NEAR(node->translation.data[1], 0.0, 1e-5);
  EXPECT_NEAR(node->translation.data[2], 0.0, 1e-5);
}

TEST_F(AlphaBlendModeTest, Node_TestBlend_Translation) {
  const Node *node = findNode(asset, "TestBlend");
  ASSERT_NE(node, nullptr);
  EXPECT_NEAR(node->translation.data[0], -1.5, 1e-5);
  EXPECT_NEAR(node->translation.data[1], 0.0, 1e-5);
  EXPECT_NEAR(node->translation.data[2], 0.0, 1e-5);
}

TEST_F(AlphaBlendModeTest, Node_TestCutoff75_Translation) {
  const Node *node = findNode(asset, "TestCutoff75");
  ASSERT_NE(node, nullptr);
  EXPECT_NEAR(node->translation.data[0], 3.0, 1e-5);
  EXPECT_NEAR(node->translation.data[1], 0.0, 1e-5);
  EXPECT_NEAR(node->translation.data[2], 0.0, 1e-5);
}

TEST_F(AlphaBlendModeTest, Node_TestCutoffDefault_Translation) {
  const Node *node = findNode(asset, "TestCutoffDefault");
  ASSERT_NE(node, nullptr);
  EXPECT_NEAR(node->translation.data[0], 1.5, 1e-5);
  EXPECT_NEAR(node->translation.data[1], 0.0, 1e-5);
  EXPECT_NEAR(node->translation.data[2], 0.0, 1e-5);
}

TEST_F(AlphaBlendModeTest, Node_GreenArrows_Translation) {
  const Node *node = findNode(asset, "GreenArrows");
  ASSERT_NE(node, nullptr);
  EXPECT_NEAR(node->translation.data[0], 0.5, 1e-5);
  EXPECT_NEAR(node->translation.data[1], 1.068, 1e-3);
  EXPECT_NEAR(node->translation.data[2], 0.005, 1e-5);
}

TEST_F(AlphaBlendModeTest, Node_DecalBlend_Rotation) {
  const Node *node = findNode(asset, "DecalBlend");
  ASSERT_NE(node, nullptr);
  EXPECT_NEAR(node->rotation.data[0], -0.472, 1e-3);
  EXPECT_NEAR(node->rotation.data[1], 0.0, 1e-5);
  EXPECT_NEAR(node->rotation.data[2], 0.0, 1e-5);
  EXPECT_NEAR(node->rotation.data[3], 0.882, 1e-3);
}

TEST_F(AlphaBlendModeTest, Node_NoChildren) {
  // All nodes in this model are root nodes with no children
  for (const auto &node : *asset->nodes) {
    EXPECT_TRUE(node.children.empty()) << "node: " << node.name;
  }
}

TEST_F(AlphaBlendModeTest, Node_NoLight) {
  for (const auto &node : *asset->nodes) {
    EXPECT_EQ(node.light, -1) << "node: " << node.name;
  }
}

// --- Meshes ------------------------------------------------------------------

TEST_F(AlphaBlendModeTest, Mesh_Names) {
  std::vector<std::string> expectedNames = {
    "BedMesh", "DecalBlendMesh", "DecalOpaqueMesh", "GreenArrowsMesh",
    "TestBlendMesh", "TestCutoff25Mesh", "TestCutoff75Mesh", "TestCutoffDefaultMesh", "TestOpaqueMesh"
  };
  for (const auto &name : expectedNames) {
    EXPECT_NE(findMesh(asset, name), nullptr) << "missing mesh: " << name;
  }
}

TEST_F(AlphaBlendModeTest, Mesh_Primitive_Attributes) {
  // Every mesh in the model has exactly one primitive with these 4 attributes
  const std::vector<std::string> expectedAttribs = {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0"};
  for (const auto &mesh : *asset->meshes) {
    ASSERT_EQ(mesh.primitives.size(), 1u) << "mesh: " << mesh.name;
    const auto &prim = mesh.primitives[0];
    for (const auto &attr : expectedAttribs) {
      EXPECT_TRUE(prim.attributes.count(attr) > 0)
          << "mesh " << mesh.name << " missing attribute: " << attr;
    }
  }
}

TEST_F(AlphaBlendModeTest, Mesh_Primitive_Mode_Triangles) {
  for (const auto &mesh : *asset->meshes) {
    EXPECT_EQ(mesh.primitives[0].mode, PrimitiveMode::pmTriangles) << "mesh: " << mesh.name;
  }
}

TEST_F(AlphaBlendModeTest, Mesh_Primitive_Has_Indices) {
  for (const auto &mesh : *asset->meshes) {
    EXPECT_GE(mesh.primitives[0].indices, 0) << "mesh: " << mesh.name << " has no index accessor";
  }
}

TEST_F(AlphaBlendModeTest, Mesh_Primitive_Has_Material) {
  for (const auto &mesh : *asset->meshes) {
    EXPECT_GE(mesh.primitives[0].material, 0) << "mesh: " << mesh.name << " has no material";
  }
}

// --- Accessors ---------------------------------------------------------------

TEST_F(AlphaBlendModeTest, Accessors_Count) {
  // 9 meshes × 5 accessors each (POSITION, NORMAL, TANGENT, TEXCOORD_0, indices) = 45
  EXPECT_EQ(asset->accessors->size(), 45);
}

TEST_F(AlphaBlendModeTest, Accessor_ReferencedBufferViews_InRange) {
  auto bvCount = (int64_t)asset->bufferViews->size();
  for (size_t i = 0; i < asset->accessors->size(); i++) {
    const auto &acc = (*asset->accessors)[i];
    EXPECT_GE(acc.bufferView, 0) << "accessor " << i;
    EXPECT_LT(acc.bufferView, bvCount) << "accessor " << i;
    EXPECT_GT(acc.count, 0u) << "accessor " << i;
  }
}

TEST_F(AlphaBlendModeTest, Accessor_ComponentTypes_Valid) {
  for (size_t i = 0; i < asset->accessors->size(); i++) {
    const auto &acc = (*asset->accessors)[i];
    int ct = (int)acc.componentType;
    // Valid glTF component types: 5120-5123, 5125, 5126
    bool valid = (ct == 5120 || ct == 5121 || ct == 5122 || ct == 5123 || ct == 5125 || ct == 5126);
    EXPECT_TRUE(valid) << "accessor " << i << " has invalid componentType: " << ct;
  }
}

// --- BufferViews -------------------------------------------------------------

TEST_F(AlphaBlendModeTest, BufferViews_ReferenceSingleBuffer) {
  for (size_t i = 0; i < asset->bufferViews->size(); i++) {
    EXPECT_EQ((*asset->bufferViews)[i].buffer, 0u) << "bufferView " << i;
  }
}

TEST_F(AlphaBlendModeTest, BufferViews_NonZeroByteLength) {
  for (size_t i = 0; i < asset->bufferViews->size(); i++) {
    EXPECT_GT((*asset->bufferViews)[i].byteLength, 0u) << "bufferView " << i;
  }
}

// --- getRuntimeInfo ----------------------------------------------------------

TEST_F(AlphaBlendModeTest, RuntimeInfo_Scene0) {
  auto info = asset->getRuntimeInfo(0);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->buffers.size(), asset->buffers->size());
  EXPECT_EQ(info->images.size(), asset->images->size());
}

TEST_F(AlphaBlendModeTest, RuntimeInfo_BuffersNeeded) {
  auto info = asset->getRuntimeInfo(0);
  ASSERT_NE(info, nullptr);
  // All scene geometry references buffer 0, so it must be needed
  EXPECT_TRUE(info->buffers[0]);
}

// ─── Embedded glTF (self-contained inline JSON) ──────────────────────────────

TEST_F(AlphaBlendModeTestEmbedded, Counts) {
  EXPECT_EQ(asset->buffers->size(), 1);
  EXPECT_EQ(asset->bufferViews->size(), 1);
  EXPECT_EQ(asset->accessors->size(), 1);
  EXPECT_EQ(asset->images->size(), 1);
  EXPECT_EQ(asset->scenes->size(), 1);
  EXPECT_EQ(asset->nodes->size(), 1);
  EXPECT_EQ(asset->meshes->size(), 1);
}

TEST_F(AlphaBlendModeTestEmbedded, Buffer_Embedded_Decoded) {
  const auto &buf = (*asset->buffers)[0];
  // URI must be cleared when data is embedded
  EXPECT_EQ(buf.uri, "");
  EXPECT_EQ(buf.byteLength, 12u);
  EXPECT_EQ(buf.data.size(), 12u);
  // All bytes should be zero (we encoded 12 zero bytes)
  for (size_t i = 0; i < buf.data.size(); i++) {
    EXPECT_EQ(buf.data[i], 0) << "byte " << i;
  }
}

TEST_F(AlphaBlendModeTestEmbedded, Image_Embedded_Decoded) {
  const auto &img = (*asset->images)[0];
  // URI must be cleared when data is embedded
  EXPECT_EQ(img.uri, "");
  // Decoded PNG data must be non-empty
  EXPECT_GT(img.data.size(), 0u);
}

TEST_F(AlphaBlendModeTestEmbedded, Scene_Properties) {
  EXPECT_EQ(asset->scene, 0);
  const auto &scene = (*asset->scenes)[0];
  EXPECT_EQ(scene.name, "EmbeddedScene");
  ASSERT_NE(scene.nodes, nullptr);
  EXPECT_EQ(scene.nodes->size(), 1u);
  EXPECT_EQ((*scene.nodes)[0], 0u);
}

TEST_F(AlphaBlendModeTestEmbedded, Node_Properties) {
  const auto &node = (*asset->nodes)[0];
  EXPECT_EQ(node.name, "EmbeddedNode");
  EXPECT_EQ(node.mesh, 0);
}

TEST_F(AlphaBlendModeTestEmbedded, Mesh_Properties) {
  const auto &mesh = (*asset->meshes)[0];
  EXPECT_EQ(mesh.name, "EmbeddedMesh");
  ASSERT_EQ(mesh.primitives.size(), 1u);
  EXPECT_EQ(mesh.primitives[0].mode, PrimitiveMode::pmTriangles);
  EXPECT_TRUE(mesh.primitives[0].attributes.count("POSITION") > 0);
}

TEST_F(AlphaBlendModeTestEmbedded, Accessor_Properties) {
  const auto &acc = (*asset->accessors)[0];
  EXPECT_EQ(acc.componentType, ComponentType::ctFloat);
  EXPECT_EQ(acc.type, AccessorType::acVec3);
  EXPECT_EQ(acc.count, 1u);
  EXPECT_EQ(acc.bufferView, 0);
}

TEST_F(AlphaBlendModeTestEmbedded, RuntimeInfo_Scene0) {
  auto info = asset->getRuntimeInfo(0);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->buffers.size(), 1u);
  EXPECT_EQ(info->images.size(), 1u);
}

// ─── Binary .glb ────────────────────────────────────────────────────────────

TEST_F(AlphaBlendModeTestGLB, Counts) {
  EXPECT_EQ(asset->buffers->size(), 1);
  // GLB packs image data into the binary chunk, adding 4 extra buffer views
  EXPECT_EQ(asset->bufferViews->size(), 49);
  EXPECT_EQ(asset->accessors->size(), 45);
  EXPECT_EQ(asset->cameras->size(), 0);
  EXPECT_EQ(asset->images->size(), 4);
  EXPECT_EQ(asset->textures->size(), 4);
  EXPECT_EQ(asset->scenes->size(), 1);
  EXPECT_EQ(asset->nodes->size(), 9);
  EXPECT_EQ(asset->samplers->size(), 1);
  EXPECT_EQ(asset->meshes->size(), 9);
  EXPECT_EQ(asset->materials->size(), 6);
  EXPECT_EQ(asset->animations->size(), 0);
  EXPECT_EQ(asset->skins->size(), 0);
  EXPECT_EQ(asset->khrLightsPunctual->size(), 0);
}

TEST_F(AlphaBlendModeTestGLB, Images_ExternalInGLB) {
  // In GLB the images are referenced via buffer views, not inline data
  for (size_t i = 0; i < asset->images->size(); i++) {
    EXPECT_EQ((*asset->images)[i].data.size(), 0) << "image " << i;
  }
}

TEST_F(AlphaBlendModeTestGLB, Material_AlphaModes) {
  EXPECT_EQ(findMaterial(asset, "MatOpaque")->alphaMode, AlphaMode::opaque);
  EXPECT_EQ(findMaterial(asset, "MatBlend")->alphaMode, AlphaMode::blend);
  EXPECT_EQ(findMaterial(asset, "MatCutoff25")->alphaMode, AlphaMode::mask);
  EXPECT_EQ(findMaterial(asset, "MatCutoff75")->alphaMode, AlphaMode::mask);
  EXPECT_EQ(findMaterial(asset, "MatCutoffDefault")->alphaMode, AlphaMode::mask);
}

TEST_F(AlphaBlendModeTestGLB, Material_AlphaCutoffs) {
  EXPECT_NEAR(findMaterial(asset, "MatCutoff25")->alphaCutoff, 0.25, 1e-6);
  EXPECT_NEAR(findMaterial(asset, "MatCutoff75")->alphaCutoff, 0.75, 1e-6);
  EXPECT_NEAR(findMaterial(asset, "MatCutoffDefault")->alphaCutoff, 0.5, 1e-6);
}

TEST_F(AlphaBlendModeTestGLB, Scene_Properties) {
  EXPECT_EQ(asset->scene, 0);
  const auto &scene = (*asset->scenes)[0];
  EXPECT_EQ(scene.name, "Scene");
  ASSERT_NE(scene.nodes, nullptr);
  EXPECT_EQ(scene.nodes->size(), 9);
}

TEST_F(AlphaBlendModeTestGLB, Node_Translations) {
  EXPECT_NEAR(findNode(asset, "TestOpaque")->translation.data[0], -3.0, 1e-5);
  EXPECT_NEAR(findNode(asset, "TestBlend")->translation.data[0], -1.5, 1e-5);
  EXPECT_NEAR(findNode(asset, "TestCutoff75")->translation.data[0], 3.0, 1e-5);
  EXPECT_NEAR(findNode(asset, "TestCutoffDefault")->translation.data[0], 1.5, 1e-5);
}

TEST_F(AlphaBlendModeTestGLB, Mesh_Primitive_Attributes) {
  const std::vector<std::string> expectedAttribs = {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0"};
  for (const auto &mesh : *asset->meshes) {
    ASSERT_EQ(mesh.primitives.size(), 1u) << "mesh: " << mesh.name;
    for (const auto &attr : expectedAttribs) {
      EXPECT_TRUE(mesh.primitives[0].attributes.count(attr) > 0)
          << "mesh " << mesh.name << " missing attribute: " << attr;
    }
  }
}

TEST_F(AlphaBlendModeTestGLB, Mesh_Primitive_Mode_Triangles) {
  for (const auto &mesh : *asset->meshes) {
    EXPECT_EQ(mesh.primitives[0].mode, PrimitiveMode::pmTriangles) << "mesh: " << mesh.name;
  }
}

TEST_F(AlphaBlendModeTestGLB, BufferViews_NonZeroByteLength) {
  for (size_t i = 0; i < asset->bufferViews->size(); i++) {
    EXPECT_GT((*asset->bufferViews)[i].byteLength, 0u) << "bufferView " << i;
  }
}

TEST_F(AlphaBlendModeTestGLB, RuntimeInfo_Scene0) {
  auto info = asset->getRuntimeInfo(0);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->buffers.size(), asset->buffers->size());
  EXPECT_EQ(info->images.size(), asset->images->size());
}

// ─── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
