#pragma once

#include <string>

namespace systems::leal::gltf
{

    /**
     * Image data used to create a texture. Image MAY be referenced by an URI
     * (or IRI) or a buffer view index.
     */
    struct Image {

        /**
         * The URI (or IRI) of the image.
         */
        std::string *uri;

        /**
         * The image’s media type. This field MUST be defined when bufferView is
         * defined.
         */
        std::string *mimeType;

        /**
         * The index of the bufferView that contains the image. This field MUST NOT
         * be defined when uri is defined.
         */
        uint64_t *bufferView;

        Image(std::string *uri, std::string *mimeType, uint64_t *bufferView) {
            this->uri = uri;
            this->mimeType = mimeType;
            this->bufferView = bufferView;
        }

        ~Image() {
            if (uri != nullptr) {
                delete uri;
            }
            if (mimeType != nullptr) {
                delete mimeType;
            }
            if (bufferView != nullptr) {
                delete bufferView;
            }
        }        
    };

}