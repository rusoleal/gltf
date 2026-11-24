// GLTFWriter: assets written as GLB read back with GLTF::loadGLB.

#include <gtest/gtest.h>
#include <gltf/gltf.hpp>
#include <gltf/writer.hpp>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace systems::leal::gltf;

namespace
{
    template <typename T>
    std::vector<T> readAccessor(const GLTF &gltf, uint64_t index)
    {
        const Accessor &accessor = (*gltf.accessors)[index];
        const BufferView &view = (*gltf.bufferViews)[accessor.bufferView];
        const Buffer &buffer = (*gltf.buffers)[view.buffer];
        uint32_t n = 1;
        switch (accessor.type)
        {
        case AccessorType::acVec2: n = 2; break;
        case AccessorType::acVec3: n = 3; break;
        case AccessorType::acVec4: n = 4; break;
        default: break;
        }
        const size_t stride = view.byteStride > 0 ? size_t(view.byteStride) : n * sizeof(T);
        std::vector<T> out(accessor.count * n);
        for (uint64_t i = 0; i < accessor.count; ++i)
            std::memcpy(&out[i * n], &buffer.data[view.byteOffset + accessor.byteOffset + i * stride], n * sizeof(T));
        return out;
    }

    uint32_t read32(const std::vector<uint8_t> &bytes, size_t offset)
    {
        return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 | uint32_t(bytes[offset + 2]) << 16 |
               uint32_t(bytes[offset + 3]) << 24;
    }

    std::shared_ptr<GLTF> load(std::vector<uint8_t> glb)
    {
        return GLTF::loadGLB(glb.data(), glb.size());
    }
}

TEST(GLTFWriter, ColoredMeshRoundTrips)
{
    const std::vector<float> positions = {0, 0, 0, 1, 0, 0, 1, 1, -0.5f, 0, 1, 0};
    const std::vector<uint8_t> colors = {255, 0, 0, 0, 255, 0, 0, 0, 255, 128, 128, 128};
    const std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    GLTFWriter writer("writer test");
    WriterPrimitive primitive;
    primitive.attributes["POSITION"] = writer.addPositions(positions.data(), 4);
    primitive.attributes["COLOR_0"] = writer.addColors(colors.data(), 4, AccessorType::acVec3);
    primitive.indices = writer.addIndices(indices.data(), indices.size());
    WriterMaterial material;
    material.name = "scan";
    material.metallicFactor = 0.0;
    material.roughnessFactor = 0.9;
    material.doubleSided = true;
    primitive.material = writer.addMaterial(material);
    WriterNode node;
    node.name = "scan node";
    node.mesh = writer.addMesh({"scan mesh", {primitive}});
    node.translation = std::array<double, 3>{1.0, 2.0, 3.0};
    node.extrasJson = R"({"frames": 375, "sensor": "depth from motion"})";
    writer.addScene({writer.addNode(node)}, "scene");

    const std::vector<uint8_t> glb = writer.toGLB();
    ASSERT_GE(glb.size(), 20u);
    EXPECT_EQ(read32(glb, 0), 0x46546C67u);
    EXPECT_EQ(read32(glb, 4), 2u);
    EXPECT_EQ(read32(glb, 8), glb.size());
    EXPECT_EQ(read32(glb, 12) % 4, 0u);
    EXPECT_EQ(glb.size() % 4, 0u);
    EXPECT_NE(writer.toJSON().find(R"("generator":"writer test")"), std::string::npos);
    EXPECT_NE(writer.toJSON().find(R"("extras":{"frames":375,"sensor":"depth from motion"})"), std::string::npos);

    const auto gltf = load(glb);
    ASSERT_NE(gltf, nullptr);
    EXPECT_EQ(gltf->scene, 0);
    ASSERT_EQ(gltf->scenes->size(), 1u);
    ASSERT_EQ(gltf->nodes->size(), 1u);
    EXPECT_EQ((*gltf->nodes)[0].mesh, 0);
    ASSERT_EQ(gltf->meshes->size(), 1u);
    const Primitive &p = (*gltf->meshes)[0].primitives[0];
    EXPECT_EQ(p.mode, PrimitiveMode::pmTriangles);
    EXPECT_EQ(p.material, 0);

    const Accessor &pos = (*gltf->accessors)[p.attributes.at("POSITION")];
    EXPECT_EQ(pos.componentType, ComponentType::ctFloat);
    EXPECT_EQ(pos.type, AccessorType::acVec3);
    ASSERT_TRUE(pos.min && pos.max);
    EXPECT_EQ(*pos.min, (std::vector<GLTF_REAL_NUMBER_TYPE>{0, 0, -0.5}));
    EXPECT_EQ(*pos.max, (std::vector<GLTF_REAL_NUMBER_TYPE>{1, 1, 0}));
    EXPECT_EQ(readAccessor<float>(*gltf, p.attributes.at("POSITION")), positions);

    const Accessor &col = (*gltf->accessors)[p.attributes.at("COLOR_0")];
    EXPECT_EQ(col.componentType, ComponentType::ctUnsignedByte);
    EXPECT_TRUE(col.normalized);
    EXPECT_EQ(readAccessor<uint8_t>(*gltf, p.attributes.at("COLOR_0")), colors);

    const Accessor &idx = (*gltf->accessors)[uint64_t(p.indices)];
    EXPECT_EQ(idx.componentType, ComponentType::ctUnsignedShort);
    const auto narrow = readAccessor<uint16_t>(*gltf, uint64_t(p.indices));
    EXPECT_EQ(std::vector<uint32_t>(narrow.begin(), narrow.end()), indices);

    const Material &m = (*gltf->materials)[0];
    EXPECT_TRUE(m.doubleSided);
    ASSERT_NE(m.pbrMetallicRoughness, nullptr);
    EXPECT_DOUBLE_EQ(m.pbrMetallicRoughness->metallicFactor, 0.0);
    EXPECT_DOUBLE_EQ(m.pbrMetallicRoughness->roughnessFactor, 0.9);
}

TEST(GLTFWriter, LargeMeshesUseUnsignedIntIndices)
{
    std::vector<float> positions(70000 * 3, 0.0f);
    for (size_t i = 0; i < 70000; ++i)
        positions[i * 3] = float(i);
    const std::vector<uint32_t> indices = {0, 1, 69999};
    GLTFWriter writer;
    WriterPrimitive primitive;
    primitive.attributes["POSITION"] = writer.addPositions(positions.data(), 70000);
    primitive.indices = writer.addIndices(indices.data(), indices.size());
    WriterNode node;
    node.mesh = writer.addMesh({"", {primitive}});
    writer.addScene({writer.addNode(node)});
    const auto gltf = load(writer.toGLB());
    const Accessor &idx = (*gltf->accessors)[uint64_t((*gltf->meshes)[0].primitives[0].indices)];
    EXPECT_EQ(idx.componentType, ComponentType::ctUnsignedInt);
    EXPECT_EQ(readAccessor<uint32_t>(*gltf, uint64_t((*gltf->meshes)[0].primitives[0].indices)), indices);
}

TEST(GLTFWriter, TexturesUnlitAndRgbaColors)
{
    const std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 13, 10, 26, 10, 1, 2, 3};  // stand-in bytes
    const std::vector<float> positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const std::vector<float> uvs = {0, 0, 1, 0, 0, 1};
    const std::vector<uint8_t> rgba = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    GLTFWriter writer;
    const int32_t image = writer.addImage(png, "image/png", "albedo");
    const int32_t texture = writer.addTexture(image, writer.addSampler());
    WriterMaterial material;
    material.baseColorTexture = texture;
    material.unlit = true;
    WriterPrimitive primitive;
    primitive.attributes["POSITION"] = writer.addPositions(positions.data(), 3);
    primitive.attributes["TEXCOORD_0"] = writer.addFloatAttribute(uvs.data(), 3, AccessorType::acVec2);
    primitive.attributes["COLOR_0"] = writer.addColors(rgba.data(), 3, AccessorType::acVec4);
    primitive.material = writer.addMaterial(material);
    WriterNode node;
    node.mesh = writer.addMesh({"", {primitive}});
    writer.addScene({writer.addNode(node)});

    const auto gltf = load(writer.toGLB());
    ASSERT_EQ(gltf->images->size(), 1u);
    const Image &img = (*gltf->images)[0];
    EXPECT_EQ(img.mimeType, "image/png");
    const BufferView &view = (*gltf->bufferViews)[uint64_t(img.bufferView)];
    EXPECT_EQ(std::vector<uint8_t>((*gltf->buffers)[0].data.begin() + view.byteOffset,
                                   (*gltf->buffers)[0].data.begin() + view.byteOffset + view.byteLength),
              png);
    ASSERT_EQ(gltf->textures->size(), 1u);
    const Material &m = (*gltf->materials)[0];
    EXPECT_TRUE(m.khrMaterialsUnlit);
    ASSERT_NE(m.pbrMetallicRoughness, nullptr);
    ASSERT_NE(m.pbrMetallicRoughness->baseColorTexture, nullptr);
    EXPECT_EQ(m.pbrMetallicRoughness->baseColorTexture->index, 0u);
    const Primitive &p = (*gltf->meshes)[0].primitives[0];
    EXPECT_EQ(readAccessor<float>(*gltf, p.attributes.at("TEXCOORD_0")), uvs);
    EXPECT_EQ(readAccessor<uint8_t>(*gltf, p.attributes.at("COLOR_0")), rgba);
    EXPECT_NE(writer.toJSON().find(R"("extensionsUsed":["KHR_materials_unlit"])"), std::string::npos);
}

TEST(GLTFWriter, NodeHierarchy)
{
    GLTFWriter writer;
    WriterNode leaf;
    leaf.name = "leaf";
    leaf.scale = std::array<double, 3>{2.0, 2.0, 2.0};
    const int32_t child = writer.addNode(leaf);
    WriterNode root;
    root.name = "root";
    root.children = {child};
    root.rotation = std::array<double, 4>{0.0, 0.0, 0.0, 1.0};
    writer.addScene({writer.addNode(root)});
    const auto gltf = load(writer.toGLB());  // no binary data: JSON chunk only
    ASSERT_EQ(gltf->nodes->size(), 2u);
    EXPECT_EQ((*gltf->nodes)[1].children, std::vector<uint64_t>{0});
    EXPECT_EQ(writer.toJSON().find("buffers"), std::string::npos);
}

TEST(GLTFWriter, RejectsInvalidReferencesAndValues)
{
    GLTFWriter writer;
    WriterPrimitive primitive;
    primitive.attributes["POSITION"] = 0;  // no accessor yet
    EXPECT_THROW(writer.addMesh({"", {primitive}}), std::out_of_range);
    EXPECT_THROW(writer.addMesh({"", {}}), std::invalid_argument);
    WriterNode node;
    node.mesh = 3;
    EXPECT_THROW(writer.addNode(node), std::out_of_range);
    node.mesh = -1;
    node.extrasJson = "[1, 2]";
    EXPECT_THROW(writer.addNode(node), std::invalid_argument);
    node.extrasJson = "{not json";
    EXPECT_THROW(writer.addNode(node), std::invalid_argument);
    WriterMaterial material;
    material.baseColorTexture = 0;
    EXPECT_THROW(writer.addMaterial(material), std::out_of_range);
    material.baseColorTexture = -1;
    material.roughnessFactor = std::nan("");
    EXPECT_THROW(writer.addMaterial(material), std::invalid_argument);
    EXPECT_THROW(writer.addScene({0}), std::out_of_range);
    const float p[3] = {0, 0, 0};
    EXPECT_THROW(writer.addFloatAttribute(p, 1, AccessorType::acMat3), std::invalid_argument);
}
