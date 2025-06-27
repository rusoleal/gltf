#include <cstdio>
#include <gtest/gtest.h>
#include <curl/curl.h>
#include <gltf/gltf.hpp>

using namespace systems::leal::gltf;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string getAsString(const std::string &url) {
    std::string buffer;
    auto curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);    
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    auto res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return buffer;
}

class AlphaBlendModeTest: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTest() {
    auto result = getAsString("https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf");
    asset = GLTF::loadGLTF(result);
  }
};

class AlphaBlendModeTestEmbedded: public testing::Test {
 protected:
  std::shared_ptr<GLTF> asset;

  AlphaBlendModeTestEmbedded() {
    auto result = getAsString("https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF-Embedded/AlphaBlendModeTest.gltf");
    asset = GLTF::loadGLTF(result);
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

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}