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

        Texture(int64_t sampler, int64_t source) {
            this->sampler = sampler;
            this->source = source;
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
