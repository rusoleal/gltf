#pragma once

/**
 * Define in your own app/library real number base type.
 * Default value = double
 */
#ifndef GLTF_REAL_NUMBER_TYPE
#define GLTF_REAL_NUMBER_TYPE double
#endif

#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include <unordered_set>
#include <gltf/runtime_info.hpp>
#include <gltf/buffer.hpp>
#include <gltf/buffer_view.hpp>
#include <gltf/accessor.hpp>
#include <gltf/image.hpp>
#include <gltf/texture.hpp>
#include <gltf/scene.hpp>
#include <gltf/node.hpp>
#include <gltf/sampler.hpp>
#include <gltf/mesh.hpp>
#include <gltf/material.hpp>
#include <gltf/animation.hpp>
#include <gltf/camera.hpp>
#include <gltf/skin.hpp>
#include <gltf/extensions/khr_lights_punctual.hpp>
#include <gltf/extensions/khr_materials_variants.hpp>
#include <gltf/extensions/khr_draco_mesh_compression.hpp>
#include <gltf/extensions/ext_meshopt_compression.hpp>

namespace systems::leal::gltf
{

    // typedef std::future<std::shared_ptr<std::vector<uint8_t>>> (* onLoadDataEvent)(const std::string &uri);

    /**
     * The root object for a glTF asset.
     */
    struct GLTF
    {

        static const std::array<char const *, 23> implementedExtensions;

        std::shared_ptr<std::vector<Buffer>> buffers;
        std::shared_ptr<std::vector<BufferView>> bufferViews;
        std::shared_ptr<std::vector<Accessor>> accessors;
        std::shared_ptr<std::vector<std::shared_ptr<Camera>>> cameras;
        std::shared_ptr<std::vector<Image>> images;
        std::shared_ptr<std::vector<Texture>> textures;
        int64_t scene;
        std::shared_ptr<std::vector<Scene>> scenes;
        std::shared_ptr<std::vector<Node>> nodes;
        std::shared_ptr<std::vector<Sampler>> samplers;
        std::shared_ptr<std::vector<Mesh>> meshes;
        std::shared_ptr<std::vector<Material>> materials;
        std::shared_ptr<std::vector<Animation>> animations;
        std::shared_ptr<std::vector<Skin>> skins;

        // Ratified Khronos extensions.
        std::shared_ptr<std::vector<KHRLightPunctual>> khrLightsPunctual;

        /// KHR_materials_variants: named variants defined at the root level.
        std::shared_ptr<std::vector<KHRMaterialVariant>> khrMaterialsVariants = nullptr;

        /// KHR_xmp_json_ld: top-level XMP packets stored as serialized JSON strings.
        std::shared_ptr<std::vector<std::string>> khrXmpPackets = nullptr;

        /// KHR_xmp_json_ld: packet index for the asset object itself. -1 if not set.
        int64_t khrXmpAssetPacket = -1;

        ~GLTF();

        std::shared_ptr<RuntimeInfo> getRuntimeInfo(uint64_t sceneIndex);

        std::string toString();

        static std::shared_ptr<GLTF> loadGLTF(const std::string &data);
        static std::shared_ptr<GLTF> loadGLB(uint8_t *data, uint64_t size);

        /**
         * Decompress any KHR_draco_mesh_compression primitives whose buffer
         * data is already available. Call this after manually filling
         * GLTF::buffers[n].data for URI-referenced external buffers.
         */
        static void decompressDraco(std::shared_ptr<GLTF> &gltf);

        /**
         * Decompress any EXT_meshopt_compression buffer views whose buffer
         * data is already available. Called automatically after loading.
         */
        static void decompressMeshopt(std::shared_ptr<GLTF> &gltf);

    private:
        void updateRuntimeInfoWithMaterial(std::shared_ptr<RuntimeInfo> runtimeInfo, Material &material);
        void updateRuntimeInfoWithNode(std::shared_ptr<RuntimeInfo> runtimeInfo, Node &node, std::unordered_set<uint64_t> &visited);
        GLTF(
            std::shared_ptr<std::vector<Buffer>> buffers,
            std::shared_ptr<std::vector<BufferView>> bufferViews,
            std::shared_ptr<std::vector<Accessor>> accessors,
            std::shared_ptr<std::vector<std::shared_ptr<Camera>>> cameras,
            std::shared_ptr<std::vector<Image>> images,
            std::shared_ptr<std::vector<Texture>> textures,
            int64_t scene,
            std::shared_ptr<std::vector<Scene>> scenes,
            std::shared_ptr<std::vector<Node>> nodes,
            std::shared_ptr<std::vector<Sampler>> samplers,
            std::shared_ptr<std::vector<Mesh>> meshes,
            std::shared_ptr<std::vector<Material>> materials,
            std::shared_ptr<std::vector<Animation>> animations,
            std::shared_ptr<std::vector<Skin>> skins,
            std::shared_ptr<std::vector<KHRLightPunctual>> khrLightsPunctual);
    };

    std::string getVersion();
}
