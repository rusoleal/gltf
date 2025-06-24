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

        GLTFChildOfRoot(const std::string &name) : name(name)
        {
        }
    };
}