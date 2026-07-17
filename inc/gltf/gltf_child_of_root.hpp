#pragma once

#include <string>

namespace systems::leal::gltf
{
    /**
     * glTF Child of Root Property.
     */
    struct GLTFChildOfRoot
    {
        /**
         * The user-defined name of this object.
         */
        std::string name;

        /**
         * glTF 2.1 unique identifier scoped to the containing file.
         * Empty when not set.
         */
        std::string uid;

        GLTFChildOfRoot(const std::string &name) : name(name)
        {
        }
    };
}