#pragma once

#include <vector>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * Joints and matrices defining a skin.
     */
    struct Skin : public GLTFChildOfRoot
    {

        /**
         * The index of the accessor containing the floating-point 4x4 inverse-bind
         * matrices.
         */
        int64_t inverseBindMatrices;

        /**
         * The index of the node used as a skeleton root.
         */
        int64_t skeleton;

        /**
         * Indices of skeleton nodes, used as joints in this skin.
         */
        std::shared_ptr<std::vector<uint64_t>> joints;

        Skin(
            const std::string &name,
            int64_t inverseBindMatrices,
            int64_t skeleton,
            std::shared_ptr<std::vector<uint64_t>> joints) : GLTFChildOfRoot(name)
        {
            this->inverseBindMatrices = inverseBindMatrices;
            this->skeleton = skeleton;
            this->joints = joints;
        }
    };
}