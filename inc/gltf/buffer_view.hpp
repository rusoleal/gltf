#pragma once

#include <vector>
#include <memory>
#include <gltf/gltf_child_of_root.hpp>
#include <gltf/extensions/ext_meshopt_compression.hpp>

namespace systems::leal::gltf
{

    /**
     * The hint representing the intended GPU buffer type to use with this buffer view.
     */
    enum class BufferViewTarget {
        undefined = 0,
        arrayBuffer = 34962,
        elementArrayBuffer = 34963
    };

    /**
     * A view into a buffer generally representing a subset of the buffer.
     */
    struct BufferView: public GLTFChildOfRoot {

        /**
         * The index of the buffer.
         */
        uint64_t buffer;

        /**
         * The offset into the buffer in bytes.
         */
        uint64_t byteOffset;

        /**
         * The length of the bufferView in bytes.
         */
        uint64_t byteLength;

        /**
         * The stride, in bytes, between vertex attributes.
         *
         * When this is not defined, data is tightly packed. When two or more
         * accessors use the same [BufferView], this field MUST be defined.
         */
        int64_t byteStride;

        /**
         * The hint representing the intended GPU buffer type to use with this
         * buffer view.
         */
        BufferViewTarget target;

        /// EXT_meshopt_compression: present when the buffer view is compressed.
        std::shared_ptr<EXTMeshoptCompression> extMeshoptCompression = nullptr;

        /// EXT_meshopt_compression: decompressed bytes (count × byteStride).
        /// Populated by GLTF::decompressMeshopt(); empty when not compressed.
        std::vector<uint8_t> decodedData;

        BufferView(
            const std::string &name,
            uint64_t buffer, 
            uint64_t byteOffset, 
            uint64_t byteLength, 
            int64_t byteStride, 
            BufferViewTarget target):GLTFChildOfRoot(name) {
            this->buffer = buffer;
            this->byteOffset = byteOffset;
            this->byteLength = byteLength;
            this->byteStride = byteStride;
            this->target = target;
        }

    };

}
