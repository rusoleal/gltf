#pragma once

#include <string>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{
    /**
     * EXT_lights_ies light profile.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_lights_ies
     */
    struct EXTLightIESProfile : public GLTFChildOfRoot
    {
        /// URI (or IRI) of the IES light profile. Empty when the profile is embedded.
        std::string uri;

        /// Media type of the embedded profile. Required when bufferView is used.
        std::string mimeType;

        /// Index of the bufferView containing the IES profile. -1 when uri is used.
        int64_t bufferView = -1;

        EXTLightIESProfile(const std::string &name) : GLTFChildOfRoot(name) {}
    };

    /**
     * EXT_lights_ies reference on a Node.
     */
    struct EXTNodeLightsIES
    {
        /// Index into GLTF::extLightsIes.
        int64_t light = -1;

        /// Non-negative intensity multiplier.
        GLTF_REAL_NUMBER_TYPE multiplier = 1.0;

        /// RGB light color in linear space.
        systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> color;

        EXTNodeLightsIES(
            int64_t light,
            GLTF_REAL_NUMBER_TYPE multiplier,
            const systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> &color)
            : light(light), multiplier(multiplier), color(color) {}
    };
}
