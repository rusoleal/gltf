#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace systems::leal::gltf
{
    /**
     * A named material variant defined at the glTF root level.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_variants
     */
    struct KHRMaterialVariant
    {
        std::string name;

        explicit KHRMaterialVariant(const std::string &name) : name(name) {}
    };

    /**
     * Maps a specific Material to a set of KHR_materials_variants variants
     * within a Primitive.
     */
    struct KHRPrimitiveMaterialsVariantsMapping
    {
        /// The index of the Material to use for the listed variants.
        int64_t material;

        /// Indices into the top-level KHR_materials_variants.variants array.
        std::vector<uint64_t> variants;

        KHRPrimitiveMaterialsVariantsMapping(int64_t material, std::vector<uint64_t> variants)
            : material(material), variants(std::move(variants)) {}
    };
}
