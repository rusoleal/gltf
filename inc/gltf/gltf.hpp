#pragma once

/**
 * Define in your own app/library real number base type.
 * Default value = double
 */
#ifndef GLTF_REAL_NUMBER_TYPE
#define GLTF_REAL_NUMBER_TYPE double
#endif

#include <memory>
#include <vector>
#include <array>
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
#include <gltf/runtime_image.hpp>
#include <gltf/extensions/khr_lights_punctual.hpp>

namespace systems::leal::gltf {
    
    //typedef std::future<std::shared_ptr<std::vector<uint8_t>>> (* onLoadDataEvent)(const std::string &uri);

    /**
     * The root object for a glTF asset.
     */
    struct GLTF {

        static const std::array<char const *, 1> implementedExtensions;

        std::shared_ptr<std::vector<Buffer>> buffers;
        std::shared_ptr<std::vector<BufferView>> bufferViews;
        std::shared_ptr<std::vector<Accessor>> accessors;
        std::shared_ptr<std::vector<Camera>> cameras;
        std::shared_ptr<std::vector<Image>> images;
        std::shared_ptr<std::vector<Texture>> textures;
        uint64_t scene;
        std::shared_ptr<std::vector<Scene>> scenes;
        std::shared_ptr<std::vector<Node>> nodes;
        std::shared_ptr<std::vector<Sampler>> samplers;
        std::shared_ptr<std::vector<Mesh>> meshes;
        std::shared_ptr<std::vector<Material>> materials;
        std::shared_ptr<std::vector<Animation>> animations;
        std::shared_ptr<std::vector<Skin>> skins;

        // Ratified Khronos extensions.
        std::shared_ptr<std::vector<KHRLightPunctual>> khrLightsPunctual;

        ~GLTF();

        std::shared_ptr<RuntimeInfo> getRuntimeInfo(uint64_t sceneIndex);

        std::string toString();

        static std::shared_ptr<GLTF> loadGLTF(const std::string &data);
        static std::shared_ptr<GLTF> loadGLB(uint8_t *data, uint64_t size);

    private:        
        GLTF(
            std::shared_ptr<std::vector<Buffer>> buffers,
            std::shared_ptr<std::vector<BufferView>> bufferViews,
            std::shared_ptr<std::vector<Accessor>> accessors,
            std::shared_ptr<std::vector<Camera>> cameras,
            std::shared_ptr<std::vector<Image>> images,
            std::shared_ptr<std::vector<Texture>> textures,
            uint64_t scene,
            std::shared_ptr<std::vector<Scene>> scenes,
            std::shared_ptr<std::vector<Node>> nodes,
            std::shared_ptr<std::vector<Sampler>> samplers,
            std::shared_ptr<std::vector<Mesh>> meshes,
            std::shared_ptr<std::vector<Material>> materials,
            std::shared_ptr<std::vector<Animation>> animations,
            std::shared_ptr<std::vector<Skin>> skins,
            std::shared_ptr<std::vector<KHRLightPunctual>> khrLightsPunctual
        );
    };
}
