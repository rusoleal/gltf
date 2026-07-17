#pragma once

#include <cstdint>
#include <vector_math/matrix4.hpp>

namespace systems::leal::gltf
{

    /**
     * Bounding volume attached to a node in glTF 2.1.
     *
     * The bounding volume is an implicit shape, optionally transformed, that
     * should completely enclose the node's content. Multiple bounding volumes
     * in a node hierarchy form a bounding volume hierarchy (BVH).
     */
    struct BoundingVolume
    {
        /**
         * Index into the top-level `shapes` array.
         */
        int64_t shape;

        /**
         * Optional local transform applied to the shape. When not present in
         * the asset the matrix is the identity.
         */
        systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> matrix;

        BoundingVolume(int64_t shape) : shape(shape)
        {
            matrix = systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE>::identity();
        }

        BoundingVolume(
            int64_t shape,
            const systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> &matrix) : shape(shape), matrix(matrix)
        {
        }
    };

}
