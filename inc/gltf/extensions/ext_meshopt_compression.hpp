#pragma once

#include <cstdint>
#include <memory>

namespace systems::leal::gltf
{

    enum class EXTMeshoptMode {
        ATTRIBUTES,  ///< Vertex attribute data
        TRIANGLES,   ///< Index buffer (triangle list)
        INDICES      ///< Index sequence
    };

    enum class EXTMeshoptFilter {
        NONE,
        OCTAHEDRAL,
        QUATERNION,
        EXPONENTIAL
    };

    /**
     * EXT_meshopt_compression extension on a BufferView.
     * Points to the compressed data inside a buffer.
     */
    struct EXTMeshoptCompression {
        int64_t  buffer;
        uint64_t byteOffset;
        uint64_t byteLength;
        uint64_t byteStride;
        uint64_t count;
        EXTMeshoptMode   mode;
        EXTMeshoptFilter filter;

        EXTMeshoptCompression(
            int64_t  buffer,
            uint64_t byteOffset,
            uint64_t byteLength,
            uint64_t byteStride,
            uint64_t count,
            EXTMeshoptMode   mode,
            EXTMeshoptFilter filter)
            : buffer(buffer)
            , byteOffset(byteOffset)
            , byteLength(byteLength)
            , byteStride(byteStride)
            , count(count)
            , mode(mode)
            , filter(filter)
        {}
    };

}
