#pragma once

#include <vector>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * The root nodes of a scene.
     */
    struct Scene : public GLTFChildOfRoot
    {

        /**
         * The indices of each root node.
         */
        std::shared_ptr<std::vector<uint64_t>> nodes;

        /// EXT_lights_image_based: index into GLTF::extLightsImageBased. -1 if not set.
        int64_t extLightImageBased = -1;

        Scene(
            const std::string &name,
            std::shared_ptr<std::vector<uint64_t>> nodes) : GLTFChildOfRoot(name)
        {
            this->nodes = nodes;
        }
    };
}