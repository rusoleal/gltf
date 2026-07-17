#pragma once

#include <vector>
#include <array>
#include <vector_math/vector3.hpp>
#include <vector_math/quaternion.hpp>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{
    /**
     * EXT_lights_image_based image-based lighting environment.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_lights_image_based
     */
    struct EXTLightImageBased : public GLTFChildOfRoot
    {
        /// Rotation of the IBL environment as a quaternion (x, y, z, w).
        systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE> rotation;

        /// Brightness multiplier.
        GLTF_REAL_NUMBER_TYPE intensity = 1.0;

        /// Spherical harmonic coefficients for irradiance up to l=2 (9x3 array).
        std::vector<std::array<GLTF_REAL_NUMBER_TYPE, 3>> irradianceCoefficients;

        /// Prefiltered cubemap mip faces. Each mip is an array of 6 image indices.
        std::vector<std::vector<uint64_t>> specularImages;

        /// Dimension (in pixels) of the first specular mip.
        uint64_t specularImageSize = 0;

        EXTLightImageBased(const std::string &name) : GLTFChildOfRoot(name) {}
    };
}
