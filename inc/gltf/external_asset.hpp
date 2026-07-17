#pragma once

#include <cstdint>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * External asset reference introduced in glTF 2.1.
     *
     * An external asset is a glTF/glB file referenced by the host file's
     * `files` array. It can be instantiated one or more times in the scene
     * hierarchy via `Node::externalAsset`.
     */
    struct ExternalAsset : public GLTFChildOfRoot
    {
        /**
         * Index into the top-level `files` array that identifies the
         * referenced glTF/glB resource.
         */
        int64_t file;

        ExternalAsset(
            const std::string &name,
            int64_t file) : GLTFChildOfRoot(name)
        {
            this->file = file;
        }
    };

}
