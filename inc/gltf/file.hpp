#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * Unified external file reference introduced in glTF 2.1.
     *
     * The `files` array is the single place where new external resource types
     * are declared. Existing `buffers` and `images` arrays are kept unchanged
     * for backward compatibility.
     */
    struct File : public GLTFChildOfRoot
    {
        /**
         * The URI (or IRI) of the file.
         */
        std::string uri;

        /**
         * The index of the bufferView that contains the file. This field MUST
         * NOT be defined when uri is defined.
         */
        int64_t bufferView;

        /**
         * The media type of the file. Required for `files` entries.
         */
        std::string mimeType;

        /**
         * Loaded file bytes, populated when the file is referenced by URI and
         * loaded through the external-resource callback.
         */
        std::vector<uint8_t> data;

        File(
            const std::string &name,
            const std::string &uri,
            int64_t bufferView,
            const std::string &mimeType,
            const std::vector<uint8_t> &data) : GLTFChildOfRoot(name)
        {
            this->uri = uri;
            this->bufferView = bufferView;
            this->mimeType = mimeType;
            this->data = data;
        }
    };

}
