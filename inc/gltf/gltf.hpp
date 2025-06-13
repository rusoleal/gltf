#pragma once

#include <memory>
#include <vector>
#include <array>
#include <future>
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

namespace systems::leal::gltf {
    
    typedef std::future<std::vector<uint8_t>> (* onLoadDataEvent)(const std::string &uri);

    /**
     * The root object for a glTF asset.
     */
    struct GLTF {

        static const std::array<char const *, 1> implementedExtensions;

        onLoadDataEvent onLoadData;

        std::vector<Buffer> buffers;
        std::vector<BufferView> bufferViews;
        std::vector<Accessor> accessors;
        std::vector<Camera> cameras;
        std::vector<Image> images;
        std::vector<Texture> textures;
        std::vector<Scene> scenes;
        std::vector<Node> nodes;
        std::vector<Sampler> samplers;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Animation> animations;
        std::vector<Skin> skins;

        // Ratified Khronos extensions.
        std::vector<KHRLightPunctual> khrLightsPunctual;

        ~GLTF();

        static std::shared_ptr<GLTF> loadGLTF(const std::string &data, onLoadDataEvent onLoadData );

    private:
        GLTF(
            const std::vector<Buffer> &buffers,
            const std::vector<BufferView> &bufferViews,
            const std::vector<Accessor> &accessors,
            const std::vector<Camera> &cameras,
            const std::vector<Image> &images,
            const std::vector<Texture> &textures,
            const std::vector<Scene> &scenes,
            const std::vector<Node> &nodes,
            const std::vector<Sampler> &samplers,
            const std::vector<Mesh> &meshes,
            const std::vector<Material> &materials,
            const std::vector<Animation> &animations,
            const std::vector<Skin> &skins,
            const std::vector<KHRLightPunctual> khrLightsPunctual
        );

    };
}
