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
        std::vector<uint64_t> *nodes;
    };
}