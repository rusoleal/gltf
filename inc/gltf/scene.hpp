#pragma once

#include <vector>

namespace systems::leal::gltf
{

    /**
     * The root nodes of a scene.
     */
    struct Scene {

        /**
         * The indices of each root node.
         */
        std::shared_ptr<std::vector<uint64_t>> nodes;

        Scene(std::shared_ptr<std::vector<uint64_t>> nodes) {
            this->nodes = nodes;
        }
    };
}