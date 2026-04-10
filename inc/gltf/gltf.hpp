#pragma once

/**
 * Define in your own app/library real number base type.
 * Default value = double
 */
#ifndef GLTF_REAL_NUMBER_TYPE
#define GLTF_REAL_NUMBER_TYPE double
#endif

#include <cstdint>
#include <future>
#include <functional>
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

        /**
         * Load a glTF asset from JSON string.
         * 
         * @deprecated Use loadGLTF(const std::string&, std::function<...>) instead.
         * This method does not load external resources or perform decompression.
         * For self-contained glTF (embedded data URIs only), use a no-op callback:
         *   loadGLTF(data, [](const std::string&) { 
         *       return std::future<std::vector<uint8_t>>{}; 
         *   })
         */
        [[deprecated("Use loadGLTF with callback instead")]]
        static std::shared_ptr<GLTF> loadGLTF(const std::string &data);

        /**
         * Load a glTF asset from JSON string with external resource loading.
         * The provided callback is invoked for each external URI that needs to be loaded.
         * The callback returns a future to support both sync and async loading patterns.
         * After all resources are loaded and futures resolved, automatic decompression is performed.
         *
         * @param data JSON string containing the glTF asset
         * @param loadCallback Callback function that receives a URI and returns a future with the loaded data
         * @return Loaded GLTF asset with all external resources and decompression applied
         */
        static std::shared_ptr<GLTF> loadGLTF(
            const std::string &data,
            std::function<std::future<std::vector<uint8_t>>(const std::string &uri)> loadCallback);

        static std::shared_ptr<GLTF> loadGLB(uint8_t *data, uint64_t size);

    private:
        /**
         * Decompress any KHR_draco_mesh_compression primitives whose buffer
         * data is already available. Called automatically by load methods.
         */
        static void decompressDraco(std::shared_ptr<GLTF> &gltf);

        /**
         * Decompress any EXT_meshopt_compression buffer views whose buffer
         * data is already available. Called automatically by load methods.
         */
        static void decompressMeshopt(std::shared_ptr<GLTF> &gltf);
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
