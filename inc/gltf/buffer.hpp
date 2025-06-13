#pragma once

#include <string>

namespace systems::leal::gltf {

    /**
     * A buffer points to binary geometry, animation, or skins.
     */
    struct Buffer {

        /**
         * The URI (or IRI) of the buffer.
         */
        std::string *uri;

        /**
         * The length of the buffer in bytes.
         */
        uint64_t byteLength;

        Buffer(std::string *uri, uint64_t byteLength) {
            this->uri = uri;
            this->byteLength = byteLength;
        }

        ~Buffer() {
            if (uri != nullptr) {
                delete uri;
            }
        }

    };

}