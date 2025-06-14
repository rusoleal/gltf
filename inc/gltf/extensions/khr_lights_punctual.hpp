#pragma once

#include <vector_math/vector3.hpp>

namespace systems::leal::gltf
{

    /// Light types.
    enum class KHRLightPunctualType
    {
        /**
         * Directional lights are light sources that act as though they are
         * infinitely far away and emit light in the direction of the local -z axis.
         *
         * This light type inherits the orientation of the [Node] that it belongs to;
         * position and scale are ignored except for their effect on the inherited
         * [Node] orientation. Because it is at an infinite distance, the light is
         * not attenuated. Directional light intensity is defined in lumens per metre
         * squared, or lux (lm/m2).
         */
        directional,

        /**
         * Point lights emit light in all directions from their position in space;
         * rotation and scale are ignored except for their effect on the inherited
         * [Node] position.
         *
         * The brightness of the light attenuates in a physically correct manner as
         * distance increases from the light's position (i.e. brightness goes like
         * the inverse square of the distance). Point light intensity is defined in
         * candela, which is lumens per square radian (lm/sr).
         */
        point,

        /**
         * Spot lights emit light in a cone in the direction of the local -z axis.
         * The angle and falloff of the cone is defined using two numbers, the
         * [innerConeAngle] and [outerConeAngle]. As with point lights, the
         * brightness also attenuates in a physically correct manner as distance
         * increases from the light's position (i.e. brightness goes like the inverse
         * square of the distance). Spot light intensity refers to the brightness
         * inside the [innerConeAngle] (and at the location of the light) and is
         * defined in candela, which is lumens per square radian (lm/sr). Engines
         * that don't support two angles for spotlights should use [outerConeAngle] as
         * the spotlight angle (leaving [innerConeAngle] to implicitly be 0).
         *
         * A spot light's position and orientation are inherited from its [Node]
         * transform. Inherited scale does not affect cone shape, and is ignored
         * except for its effect on position and orientation.
         */
        spot,
    };

    /**
     * Light definition according to KHR_lights_punctual extension.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual
     */
    struct KHRLightPunctual
    {

        systems::leal::vector_math::Vector3 color;

        double intensity;

        KHRLightPunctualType type;

        double *range;

        double innerConeAngle;

        double outerConeAngle;
    };

}