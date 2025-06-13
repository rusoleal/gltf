#pragma once

#include <vector>

namespace systems::leal::gltf {

    /**
     * Joints and matrices defining a skin.
     */
    struct Skin {

        /**
         * The index of the accessor containing the floating-point 4x4 inverse-bind
         * matrices.
         */
        uint64_t *inverseBindMatrices;

        /**
         * The index of the node used as a skeleton root.
         */
        uint64_t *skeleton;

        /**
         * Indices of skeleton nodes, used as joints in this skin.
         */
        std::shared_ptr<std::vector<uint64_t>> joints;

        Skin(uint64_t *inverseBindMatrices, uint64_t *skeleton, std::shared_ptr<std::vector<uint64_t>> joints) {
            this->inverseBindMatrices = inverseBindMatrices;
            this->skeleton = skeleton;
            this->joints = joints;
        }

    };
}