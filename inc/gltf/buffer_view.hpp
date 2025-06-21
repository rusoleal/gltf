#pragma once

namespace systems::leal::gltf
{

    /**
     * The hint representing the intended GPU buffer type to use with this buffer view.
     */
    enum class BufferViewTarget {
        arrayBuffer = 34962,
        elementArrayBuffer = 34963
    };

    /**
     * A view into a buffer generally representing a subset of the buffer.
     */
    struct BufferView {

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
        uint64_t byteStride;

        /**
         * The hint representing the intended GPU buffer type to use with this
         * buffer view.
         */
        BufferViewTarget* target;


        BufferView(uint64_t buffer, uint64_t byteOffset, uint64_t byteLength, uint64_t byteStride, BufferViewTarget* target) {
            this->buffer = buffer;
            this->byteOffset = byteOffset;
            this->byteLength = byteLength;
            this->byteStride = byteStride;
            this->target = target;
        }

        ~BufferView() {
            if (target != nullptr) {
                delete target;
            }
        }


    };

}
