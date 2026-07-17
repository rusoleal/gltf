#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace systems::leal::gltf
{
    /**
     * Line string encoded by EXT_mesh_primitive_edge_visibility.
     */
    struct EXTMeshPrimitiveEdgeVisibilityLineString
    {
        /// Accessor index containing the vertex indices for the line strings.
        int64_t indices = -1;

        /// Material index used to draw the edges. -1 if omitted.
        int64_t material = -1;
    };

    /**
     * EXT_mesh_primitive_edge_visibility primitive extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_mesh_primitive_edge_visibility
     */
    struct EXTMeshPrimitiveEdgeVisibility
    {
        /// Accessor index encoding edge visibility as a 2-bit bitfield per edge.
        int64_t visibility = -1;

        /// Material index used to draw the edges. -1 if omitted.
        int64_t material = -1;

        /// Accessor index with silhouette normals for visibility value 1 edges.
        int64_t silhouetteNormals = -1;

        /// Edges encoded as explicit line strings.
        std::vector<EXTMeshPrimitiveEdgeVisibilityLineString> lineStrings;
    };
}
