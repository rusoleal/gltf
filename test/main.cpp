#include <cstdio>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

class AlphaBlendModeTest: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTest() {
    //asset = GLTF::loadGLTF(result);
  }
};

class AlphaBlendModeTestEmbedded: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTestEmbedded() {
    //auto result = getAsString("https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF-Embedded/AlphaBlendModeTest.gltf");
    //asset = GLTF::loadGLTF(result);
  }
};

/*TEST_F(AlphaBlendModeTest, AlphaBlendModeTest_gltf) {
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
}*/

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}