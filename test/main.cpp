#include <fstream>
#include <streambuf>
#include <cstdio>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

std::string loadFileAsString(const std::string &filename) {
    std::ifstream t(filename);
    std::string str;

    t.seekg(0, std::ios::end);   
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
                std::istreambuf_iterator<char>());  
    return str;  
}

class AlphaBlendModeTest: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTest() {
    auto result = loadFileAsString("AlphaBlendModeTest.gltf");
    asset = GLTF::loadGLTF(result);
  }
};

class AlphaBlendModeTestEmbedded: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTestEmbedded() {
    auto result = loadFileAsString("AlphaBlendModeTestEmbedded.gltf");
    asset = GLTF::loadGLTF(result);
  }
};

class AlphaBlendModeTestGLB: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTestGLB() {
    auto result = loadFileAsString("AlphaBlendModeTest.glb");
    asset = GLTF::loadGLB((uint8_t *)result.c_str(),result.size());
  }
};

TEST_F(AlphaBlendModeTest, AlphaBlendModeTest_gltf) {
  EXPECT_EQ(asset->buffers->size(), 1);
  EXPECT_EQ(asset->bufferViews->size(), 45);
  EXPECT_EQ(asset->accessors->size(), 45);
  EXPECT_EQ(asset->cameras->size(), 0);
  EXPECT_EQ(asset->images->size(), 4);
  EXPECT_EQ((*asset->images)[0].data.size(), 0);
  EXPECT_EQ((*asset->images)[1].data.size(), 0);
  EXPECT_EQ((*asset->images)[2].data.size(), 0);
  EXPECT_EQ((*asset->images)[3].data.size(), 0);
  EXPECT_EQ(asset->textures->size(), 4);
  EXPECT_EQ(asset->scenes->size(), 1);
  EXPECT_EQ(asset->nodes->size(), 9);
  EXPECT_EQ(asset->samplers->size(), 1);
  EXPECT_EQ(asset->meshes->size(), 9);
  EXPECT_EQ(asset->materials->size(), 6);
  EXPECT_EQ(asset->khrLightsPunctual->size(), 0);
}

TEST_F(AlphaBlendModeTestEmbedded, AlphaBlendModeTest_gltf) {
  EXPECT_EQ(asset->buffers->size(), 1);
  EXPECT_EQ(asset->bufferViews->size(), 45);
  EXPECT_EQ(asset->accessors->size(), 45);
  EXPECT_EQ(asset->cameras->size(), 0);
  EXPECT_EQ(asset->images->size(), 4);
  EXPECT_NE((*asset->images)[0].data.size(), 0);
  EXPECT_NE((*asset->images)[1].data.size(), 0);
  EXPECT_NE((*asset->images)[2].data.size(), 0);
  EXPECT_NE((*asset->images)[3].data.size(), 0);
  EXPECT_EQ(asset->textures->size(), 4);
  EXPECT_EQ(asset->scenes->size(), 1);
  EXPECT_EQ(asset->nodes->size(), 9);
  EXPECT_EQ(asset->samplers->size(), 1);
  EXPECT_EQ(asset->meshes->size(), 9);
  EXPECT_EQ(asset->materials->size(), 6);
  EXPECT_EQ(asset->khrLightsPunctual->size(), 0);
}

TEST_F(AlphaBlendModeTestGLB, AlphaBlendModeTest_gltf) {
  EXPECT_EQ(asset->buffers->size(), 1);
  EXPECT_EQ(asset->bufferViews->size(), 49);
  EXPECT_EQ(asset->accessors->size(), 45);
  EXPECT_EQ(asset->cameras->size(), 0);
  EXPECT_EQ(asset->images->size(), 4);
  EXPECT_EQ((*asset->images)[0].data.size(), 0);
  EXPECT_EQ((*asset->images)[1].data.size(), 0);
  EXPECT_EQ((*asset->images)[2].data.size(), 0);
  EXPECT_EQ((*asset->images)[3].data.size(), 0);
  EXPECT_EQ(asset->textures->size(), 4);
  EXPECT_EQ(asset->scenes->size(), 1);
  EXPECT_EQ(asset->nodes->size(), 9);
  EXPECT_EQ(asset->samplers->size(), 1);
  EXPECT_EQ(asset->meshes->size(), 9);
  EXPECT_EQ(asset->materials->size(), 6);
  EXPECT_EQ(asset->khrLightsPunctual->size(), 0);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}