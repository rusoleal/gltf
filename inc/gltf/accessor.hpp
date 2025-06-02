#pragma once

#include <vector>

namespace systems::leal::gltf
{

    /**
     * The datatype of the accessor’s components.
     */
    enum class ComponentType {
        ctByte = 5120,
        ctUnsignedByte = 5121,
        ctShort = 5122,
        ctUnsignedShort = 5123,
        ctUnsignedInt = 5125,
        ctFloat = 5126,
    };


    /**
     * Specifies if the accessor’s elements are scalars, vectors, or matrices.
     */
    enum class AccessorType {
        acScalar,
        acVec2,
        acVec3,
        acVec4,
        acMat2,
        acMat3,
        acMat4
    };

    /**
     * A typed view into a buffer view that contains raw binary data.
     */
    struct Accessor {

        /**
         * The index of the [BufferView]. When undefined, the accessor MUST be
         * initialized with zeros; [sparse] property or extensions MAY override zeros
         * with actual values.
         */
        uint64_t *bufferView;
        
        /**
         * The offset relative to the start of the [BufferView] in bytes.
         *
         * This MUST be a multiple of the size of the component datatype. This
         * property MUST NOT be defined when [bufferView] is undefined.
         */
        uint64_t byteOffset;

        /**
         * The datatype of the accessor’s components.
         *
         * [ComponentType.unsignedInt] type MUST NOT be used for any accessor that is
         * not referenced by [Primitive.indices].
         */
        ComponentType componentType;

        /**
         * Specifies whether integer data values are normalized (true) to [0, 1]
         * (for unsigned types) or to [-1, 1] (for signed types) when they are
         * accessed.
         *
         * This property MUST NOT be set to true for accessors with
         * [ComponentType.float] or [ComponentType.unsignedInt] component type.
         */
        bool normalized;

        /**
         * The number of elements referenced by this accessor, not to be confused
         * with the number of bytes or number of components.
         */
        uint64_t count;

        /**
         * Specifies if the accessor’s elements are scalars, vectors, or matrices.
         */
        AccessorType type;

        /**
         * Maximum value of each component in this accessor. Array elements MUST be
         * treated as having the same data type as accessor’s [componentType].
         * Both min and max arrays have the same length. The length is determined by
         * the value of the [type] property; it can be 1, 2, 3, 4, 9, or 16.
         *
         * [normalized] property has no effect on array values: they always correspond
         * to the actual values stored in the buffer. When the accessor is sparse,
         * this property MUST contain maximum values of accessor data with sparse
         * substitution applied.
         */
        std::vector<uint8_t> *max;

        /**
         * Minimum value of each component in this accessor. Array elements MUST be
         * treated as having the same data type as accessor’s [componentType].
         * Both min and max arrays have the same length. The length is determined by
         * the value of the [type] property; it can be 1, 2, 3, 4, 9, or 16.
         *
         * [normalized] property has no effect on array values: they always correspond
         * to the actual values stored in the buffer. When the accessor is sparse,
         * this property MUST contain minimum values of accessor data with sparse
         * substitution applied.
         */
        std::vector<uint8_t> *min;

        // TODO sparse

        /*Accessor(
            uint64_t *bufferView, 
            uint64_t byteOffset, 
            ComponentType componentType, 
            bool normalized,
            uint64_t count,
            AccessorType type) {

                this->bufferView = bufferView;
                this->byteOffset = byteOffset;
                this->componentType = componentType;
                this->normalized = normalized;
                this->count = count;
                this->type = type;
        }*/

        ~Accessor() {
            if (bufferView != nullptr) {
                delete bufferView;
            }
        }

    };
}