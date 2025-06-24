#pragma once

#include <string>
#include <vector>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * The descriptor of the animated property.
     */
    struct AnimationChannelTarget
    {

        /**
         * The index of the [Node] to animate. When undefined, the animated object
         * MAY be defined by an extension.
         */
        uint64_t node;

        /**
         * The name of the node’s TRS property to animate, or the "weights" of the
         * Morph Targets it instantiates. For the "translation" property, the values
         * that are provided by the sampler are the translation along the X, Y, and
         * Z axes. For the "rotation" property, the values are a quaternion in the
         * order (x, y, z, w), where w is the scalar. For the "scale" property, the
         * values are the scaling factors along the X, Y, and Z axes.
         */
        std::string path;
    };

    /**
     * An animation channel combines an animation sampler with a target property
     * being animated.
     */
    struct AnimationChannel
    {

        /**
         * The index of a [Sampler] in this animation used to compute the value for
         * the target, e.g., a [Node]’s translation, rotation, or scale (TRS).
         */
        uint64_t sampler;

        /**
         * The descriptor of the animated property.
         */
        AnimationChannelTarget target;
    };

    /**
     * Interpolation algorithm.
     */
    enum class AnimationInterpolation
    {

        /**
         * The animated values are linearly interpolated between keyframes. When
         * targeting a rotation, spherical linear interpolation (slerp) SHOULD be
         * used to interpolate quaternions. The number of output elements MUST equal
         * the number of input elements.
         */
        aiLinear,

        /**
         * The animated values remain constant to the output of the first keyframe,
         * until the next keyframe. The number of output elements MUST equal the
         * number of input elements.
         */
        aiStep,

        /**
         * The animation’s interpolation is computed using a cubic spline with
         * specified tangents. The number of output elements MUST equal three times
         * the number of input elements. For each input element, the output stores
         * three elements, an in-tangent, a spline vertex, and an out-tangent.
         * There MUST be at least two keyframes when using this interpolation.
         */
        aiCubicSpline,
    };

    /**
     * An animation sampler combines timestamps with a sequence of output values
     * and defines an interpolation algorithm.
     */
    struct AnimationSampler
    {

        /**
         * The index of an [Accessor] containing keyframe timestamps.
         *
         * The accessor MUST be of scalar type with floating-point components. The
         * values represent time in seconds with time[0] >= 0.0, and strictly
         * increasing values, i.e., time[n + 1] > time[n].
         */
        uint64_t input;

        /**
         * Interpolation algorithm.
         */
        AnimationInterpolation interpolation;

        /**
         * The index of an [Accessor], containing keyframe output values.
         */
        uint64_t output;
    };

    /**
     * A keyframe animation.
     */
    struct Animation : public GLTFChildOfRoot
    {

        /**
         * An array of [AnimationChannel]. An [AnimationChannel] combines an
         * animation sampler with a target property being animated. Different
         * channels of the same animation MUST NOT have the same targets.
         */
        std::vector<AnimationChannel> channels;

        /**
         * An array of [AnimationSampler]. An [AnimationSampler] combines timestamps
         * with a sequence of output values and defines an interpolation algorithm.
         */
        std::vector<AnimationSampler> samplers;

        Animation(
            const std::string &name,
            const std::vector<AnimationChannel> &channels,
            const std::vector<AnimationSampler> &samplers) : GLTFChildOfRoot(name)
        {
            this->channels = channels;
            this->samplers = samplers;
        }
    };
}
