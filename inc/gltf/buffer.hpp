#pragma once

#include <string>
#include <vector>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * A buffer points to binary geometry, animation, or skins.
     */
    struct Buffer : public GLTFChildOfRoot
    {

        /**
         * The URI (or IRI) of the buffer.
         */
        std::string uri;

        /**
         * The length of the buffer in bytes.
         */
        uint64_t byteLength;

        /**
         * Embedded data:uri or glb format.
         *
         * Only available for embedded content. Length must fit [byteLength] property.
         */
        std::vector<uint8_t> data;

        Buffer(
            const std::string &name,
            const std::string &uri,
            uint64_t byteLength,
            std::vector<uint8_t> &data) : GLTFChildOfRoot(name)
        {
            this->uri = uri;
            this->byteLength = byteLength;
            this->data = data;
        }
    };

}