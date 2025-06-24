#pragma once

#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * Magnification/minification filter modes.
     */
    enum class FilterMode
    {
        fmUnknown = 0,
        fmNearest = 9728,
        fmLinear = 9729,
        fmNearestMipmapNearest = 9984,
        fmLinearMipmapNearest = 9985,
        fmNearestMipmapLinear = 9986,
        fmLinearMipmapLinear = 9987,
    };

    /**
     * Wrapping mode.
     */
    enum class WrapMode
    {
        clampToEdge = 33071,
        mirroredRepeat = 33648,
        repeat = 10497
    };

    /**
     * Texture sampler properties for filtering and wrapping modes.
     */
    struct Sampler : public GLTFChildOfRoot
    {

        /**
         * Magnification filter.
         */
        FilterMode magFilter;

        /**
         * Minification filter.
         */
        FilterMode minFilter;

        /**
         * S (U) wrapping mode. All valid values correspond to WebGL enums.
         */
        WrapMode wrapS;

        /**
         * T (V) wrapping mode.
         */
        WrapMode wrapT;

        Sampler(
            const std::string &name,
            FilterMode magFilter,
            FilterMode minFilter,
            WrapMode wrapS,
            WrapMode wrapT) : GLTFChildOfRoot(name)
        {
            this->magFilter = magFilter;
            this->minFilter = minFilter;
            this->wrapS = wrapS;
            this->wrapT = wrapT;
        }

        /*~Sampler() {
            if (magFilter != nullptr) {
                delete magFilter;
            }
            if (minFilter != nullptr) {
                delete minFilter;
            }
        }*/
    };
}
