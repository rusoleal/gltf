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
        std::string uri;

        /**
         * The image’s media type. This field MUST be defined when bufferView is
         * defined.
         */
        std::string mimeType;

        /**
         * The index of the bufferView that contains the image. This field MUST NOT
         * be defined when uri is defined.
         */
        int64_t bufferView;

        /**
         * Embedded data:uri or glb format.
         * 
         * Only available for embedded content. Length must fit [byteLength] property.
         */
        std::vector<uint8_t> data;

        Image(const std::string &uri, const std::string &mimeType, int64_t bufferView, const std::vector<uint8_t> &data) {
            this->uri = uri;
            this->mimeType = mimeType;
            this->bufferView = bufferView;
            this->data = data;
        }

        /*~Image() {
            if (uri != nullptr) {
                delete uri;
            }
            if (mimeType != nullptr) {
                delete mimeType;
            }
            if (bufferView != nullptr) {
                delete bufferView;
            }
        }*/       
    };

}