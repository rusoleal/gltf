#pragma once

#include <cstdint>
#include <vector_math/vector2.hpp>

namespace systems::leal::gltf
{
    /**
     * KHR_texture_transform extension data for TextureInfo.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_texture_transform
     */
    struct KHRTextureTransform
    {
        /// UV coordinate offset as a factor of the texture dimensions. Default [0, 0].
        systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> offset;

        /// UV rotation in radians counter-clockwise around the origin. Default 0.
        GLTF_REAL_NUMBER_TYPE rotation;

        /// UV scale factors. Default [1, 1].
        systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> scale;

        /// Overrides the texCoord of the parent TextureInfo when set. -1 = not set.
        int64_t texCoord;

        KHRTextureTransform(
            const systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> &offset,
            GLTF_REAL_NUMBER_TYPE rotation,
            const systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> &scale,
            int64_t texCoord)
            : offset(offset), rotation(rotation), scale(scale), texCoord(texCoord) {}
    };
}
