#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace systems::leal::gltf
{

    /**
     * The topology type of primitives to render.
     */
    enum PrimitiveMode {
        pmPoints = 0,
        pmLines = 1,
        pmLineLoop = 2,
        pmLineStrip = 3,
        pmTriangles = 4,
        pmTriangleStrip = 5,
        pmTriangleFan = 6,
    };

    /**
     * Geometry to be rendered with the given [Material].
     */
    struct Primitive {
        
        /**
         * A plain JSON object, where each key corresponds to a mesh attribute
         * semantic and each value is the index of the accessor containing
         * attribute’s data.
         */
        std::unordered_map<std::string, uint64_t> attributes;

        /**
         * The index of the [Accessor] that contains the vertex indices. When this is
         * undefined, the primitive defines non-indexed geometry. When defined, the
         * [Accessor] MUST have SCALAR type and an unsigned integer component type.
         */
        int64_t indices;

        /**
         * The index of the [Material] to apply to this primitive when rendering.
         */
        int64_t material;

        /**
         * The topology type of primitives to render.
         */
        PrimitiveMode mode;

        /**
         * An array of morph targets.
         */
        std::vector<uint8_t> targets;

        Primitive(
            const std::unordered_map<std::string, uint64_t> &attributes,
            int64_t indices,
            int64_t material,
            PrimitiveMode mode,
            std::vector<uint8_t> targets
        ) {
            this->attributes = attributes;
            this->indices = indices;
            this->material = material;
            this->mode = mode;
            this->targets = targets;
        }
    };

    /**
     * A set of primitives to be rendered. Its global transform is defined by a
     * [Node] that references it.
     */
    struct Mesh {
        
        /**
         * An array of primitives, each defining geometry to be rendered.
         */
        std::vector<Primitive> primitives;

        /**
         * Array of weights to be applied to the morph targets. The number of array
         * elements MUST match the number of morph targets.
         */
        std::vector<GLTF_REAL_NUMBER_TYPE> weights;

        Mesh(const std::vector<Primitive> primitives, const std::vector<GLTF_REAL_NUMBER_TYPE> &weights) {
            this->primitives = primitives;
            this->weights = weights;
        }
    };

}
