#pragma once

#include <unordered_map>

namespace systems::leal::gltf
{
    /**
     * EXT_mesh_gpu_instancing extension
     */
    struct EXTMeshGpuInstancing {

        /**
         * The attributes section contains accessor ids for the TRANSLATION, ROTATION
         * and SCALE attribute buffers, all of which are optional.
         */
        std::unordered_map<std::string,uint64_t> attributes;

        EXTMeshGpuInstancing(){}
    };
}