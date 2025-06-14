#pragma once

#include <vector>
#include <vector_math/vector3.hpp>
#include <vector_math/quaternion.hpp>
#include <vector_math/matrix4.hpp>

namespace systems::leal::gltf
{

    /**
     * A node in the node hierarchy.
     *
     * When the node contains skin, all mesh.primitives MUST contain JOINTS_0 and
     * WEIGHTS_0 attributes. A node MAY have either a matrix or any combination of
     * translation/rotation/scale (TRS) properties. TRS properties are converted
     * to matrices and postmultiplied in the T * R * S order to compose the
     * transformation matrix; first the scale is applied to the vertices, then the
     * rotation, and then the translation. If none are provided, the transform is
     * the identity.
     *
     * When a node is targeted for animation (referenced by an
     * [AnimationChannelTarget]), matrix MUST NOT be present.
     */
    struct Node {

        /**
         * The index of the [Camera] referenced by this node.
         */
        uint64_t *camera;

        /**
         * The indices of this node’s children.
         */
        std::vector<uint64_t> *children;

        /**
         * The index of the skin referenced by this node. When a skin is referenced
         * by a node within a [Scene], all joints used by the skin MUST belong to the
         * same scene. When defined, [mesh] MUST also be defined.
         */
        uint64_t *skin;

        /**
         * A floating-point 4x4 transformation matrix stored in column-major order.
         */
        systems::leal::vector_math::Matrix4 matrix;

        /**
         * The index of the [Mesh] in this node.
         */
        uint64_t *mesh;

        /**
         * The node’s unit quaternion rotation in the order (x, y, z, w), where w is
         * the scalar.
         */
        systems::leal::vector_math::Quaternion rotation;

        /**
         * The node’s non-uniform scale, given as the scaling factors along the x, y,
         * and z axes.
         */
        systems::leal::vector_math::Vector3 scale;

        /**
         * The node’s translation along the x, y, and z axes.
         */
        systems::leal::vector_math::Vector3 translation;

        /**
         * The weights of the instantiated morph target. The number of array elements
         * MUST match the number of morph targets of the referenced mesh. When
         * defined, mesh MUST also be defined.
         */
        std::vector<double> weights;

        Node(
            uint64_t *camera,
            std::vector<uint64_t> *children,
            uint64_t *skin,
            systems::leal::vector_math::Matrix4 matrix,
            uint64_t *mesh,
            systems::leal::vector_math::Quaternion rotation,
            systems::leal::vector_math::Vector3 scale,
            systems::leal::vector_math::Vector3 translation,
            std::vector<double> weights
        ) {
            this->camera =  camera;
            this->children = children;
            this->skin =  skin;
            this->matrix = matrix;
            this->mesh =  mesh;
            this->rotation = rotation;
            this->scale = scale;
            this->translation=  translation;
            this->weights = weights;
        }
    };
}
