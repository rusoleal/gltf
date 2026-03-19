#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace systems::leal::gltf
{
    /**
     * KHR_draco_mesh_compression extension for Primitive.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_draco_mesh_compression
     *
     * Stores the compressed buffer reference and the decoded geometry after parsing.
     * decodedIndices and decodedAttributes are populated automatically when buffer
     * data is available (inline base64 or GLB binary chunk). Primitives backed by
     * URI-referenced buffers will have empty decoded fields until the caller loads
     * those buffers externally.
     */
    struct KHRDracoMeshCompression
    {
        /// Index of the BufferView containing the Draco-compressed mesh data.
        int64_t bufferView;

        /// Maps glTF attribute semantics (e.g. "POSITION") to Draco unique attribute IDs.
        std::unordered_map<std::string, uint64_t> attributes;

        /// Decoded triangle indices (uint32, 3 per face). Populated after parsing
        /// when buffer data is available; empty otherwise.
        std::vector<uint32_t> decodedIndices;

        /// Decoded attribute data keyed by attribute semantic (e.g. "POSITION").
        /// Each entry holds tightly-packed raw bytes in the format described by the
        /// corresponding Accessor (componentType × components-per-element, num_points elements).
        std::unordered_map<std::string, std::vector<uint8_t>> decodedAttributes;

        KHRDracoMeshCompression(
            int64_t bufferView,
            std::unordered_map<std::string, uint64_t> attributes)
            : bufferView(bufferView), attributes(std::move(attributes)) {}
    };
}
