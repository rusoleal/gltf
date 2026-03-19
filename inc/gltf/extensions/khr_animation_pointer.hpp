#pragma once

#include <string>

namespace systems::leal::gltf
{
    /**
     * KHR_animation_pointer extension for AnimationChannelTarget.
     * Allows animating any property in the glTF document via a JSON pointer.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_animation_pointer
     *
     * When present, AnimationChannelTarget.path is "pointer" and
     * AnimationChannelTarget.node is -1 (undefined).
     */
    struct KHRAnimationPointer
    {
        /// A JSON pointer (RFC 6901) identifying the animated property,
        /// e.g. "/nodes/0/translation" or "/materials/2/pbrMetallicRoughness/baseColorFactor".
        std::string pointer;

        explicit KHRAnimationPointer(const std::string &pointer) : pointer(pointer) {}
    };
}
