#pragma once

#include <cstdint>
#include <vector>

namespace systems::leal::gltf
{
    /**
     * Primitive group defined by EXT_mesh_primitive_restart.
     */
    struct EXTMeshPrimitiveRestartGroup
    {
        /// Indices of the primitives that are combined and drawn together.
        std::vector<uint64_t> primitives;

        /// Accessor index containing the combined indices with restart values.
        int64_t indices = -1;
    };

    /**
     * EXT_mesh_primitive_restart primitive extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_mesh_primitive_restart
     */
    struct EXTMeshPrimitiveRestart
    {
        /// Groups of primitives drawn using a single index buffer with restart values.
        std::vector<EXTMeshPrimitiveRestartGroup> primitiveGroups;
    };
}
