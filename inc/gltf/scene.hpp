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

        Scene(
            const std::string &name,
            std::shared_ptr<std::vector<uint64_t>> nodes) : GLTFChildOfRoot(name)
        {
            this->nodes = nodes;
        }
    };
}