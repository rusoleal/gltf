#pragma once

#include <cstdint>

namespace systems::leal::gltf
{

    /**
     * A texture and its sampler.
     */
    struct Texture {

        /**
         * The index of the sampler used by this texture. When undefined, a sampler
         * with repeat wrapping and auto filtering SHOULD be used.
         */
        int64_t sampler;

        /**
         * The index of the image used by this texture. When undefined, an extension
         * or other mechanism SHOULD supply an alternate texture source, otherwise
         * behavior is undefined.
         */
        int64_t source;

        /**
         * The index of the webp image populated by EXT_texture_webp extension.
         * Undefined value = -1
         */
        int64_t ext_texture_webp;

        Texture(int64_t sampler, int64_t source, int64_t ext_texture_webp) {
            this->sampler = sampler;
            this->source = source;
            this->ext_texture_webp = ext_texture_webp;
        }

        /*~Texture() {
            if (sampler != nullptr) {
                delete sampler;
            }
            if (source != nullptr) {
                delete source;
            }
        }*/

    };
}
