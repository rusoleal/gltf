#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <gltf/gltf_child_of_root.hpp>
#include <gltf/extensions/khr_materials_variants.hpp>
#include <gltf/extensions/khr_draco_mesh_compression.hpp>
#include <gltf/extensions/khr_gaussian_splatting.hpp>
#include <gltf/extensions/ext_mesh_primitive_edge_visibility.hpp>
#include <gltf/extensions/ext_mesh_primitive_restart.hpp>

namespace systems::leal::gltf
{

    /**
     * The topology type of primitives to render.
     */
    enum PrimitiveMode
    {
        pmPoints = 0,
        pmLines = 1,
        pmLineLoop = 2,
        pmLineStrip = 3,
        pmTriangles = 4,
        pmTriangleStrip = 5,
        pmTriangleFan = 6,
    };

    /**
     * Geometry to be rendered with the given [Material].
     */
    struct Primitive
    {

        /**
         * A plain JSON object, where each key corresponds to a mesh attribute
         * semantic and each value is the index of the accessor containing
         * attribute’s data.
         */
        std::unordered_map<std::string, uint64_t> attributes;

        /**
         * The index of the [Accessor] that contains the vertex indices. When this is
         * undefined, the primitive defines non-indexed geometry. When defined, the
         * [Accessor] MUST have SCALAR type and an unsigned integer component type.
         */
        int64_t indices;

        /**
         * The index of the [Material] to apply to this primitive when rendering.
         */
        int64_t material;

        /**
         * The topology type of primitives to render.
         */
        PrimitiveMode mode;

        /**
         * An array of morph targets. Each entry maps an attribute semantic
         * (e.g. "POSITION", "NORMAL", "TANGENT") to the index of an [Accessor]
         * containing the displacement values for that attribute.
         */
        std::vector<std::unordered_map<std::string, uint64_t>> targets;

        /**
         * KHR_materials_variants mappings for this primitive.
         * Each entry maps a material index to a set of variant indices.
         */
        std::vector<KHRPrimitiveMaterialsVariantsMapping> khrMaterialsVariantsMappings;

        /// KHR_draco_mesh_compression: compressed geometry data and decoded output.
        /// nullptr when the extension is absent.
        std::shared_ptr<KHRDracoMeshCompression> khrDracoMeshCompression = nullptr;

        /// KHR_gaussian_splatting: Gaussian splat rendering hints. nullptr if absent.
        std::shared_ptr<KHRGaussianSplatting> khrGaussianSplatting = nullptr;

        /// EXT_mesh_primitive_edge_visibility: edge visibility data. nullptr if absent.
        std::shared_ptr<EXTMeshPrimitiveEdgeVisibility> extMeshPrimitiveEdgeVisibility = nullptr;

        /// EXT_mesh_primitive_restart: primitive restart groups. nullptr if absent.
        std::shared_ptr<EXTMeshPrimitiveRestart> extMeshPrimitiveRestart = nullptr;

        Primitive(
            const std::unordered_map<std::string, uint64_t> &attributes,
            int64_t indices,
            int64_t material,
            PrimitiveMode mode,
            std::vector<std::unordered_map<std::string, uint64_t>> targets)
        {
            this->attributes = attributes;
            this->indices = indices;
            this->material = material;
            this->mode = mode;
            this->targets = std::move(targets);
        }
    };

    /**
     * EXT_mesh_manifold mesh extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_mesh_manifold
     */
    struct EXTMeshManifold
    {
        /// A full primitive describing the manifold geometry.
        Primitive manifoldPrimitive;

        /// Accessor index containing vertex sparse indices for merging. -1 if absent.
        int64_t mergeIndices = -1;

        /// Accessor index containing vertex sparse values for merging. -1 if absent.
        int64_t mergeValues = -1;

        EXTMeshManifold(
            const Primitive &manifoldPrimitive,
            int64_t mergeIndices,
            int64_t mergeValues)
            : manifoldPrimitive(manifoldPrimitive),
              mergeIndices(mergeIndices),
              mergeValues(mergeValues) {}
    };

    /**
     * A set of primitives to be rendered. Its global transform is defined by a
     * [Node] that references it.
     */
    struct Mesh : public GLTFChildOfRoot
    {

        /**
         * An array of primitives, each defining geometry to be rendered.
         */
        std::vector<Primitive> primitives;

        /**
         * Array of weights to be applied to the morph targets. The number of array
         * elements MUST match the number of morph targets.
         */
        std::vector<GLTF_REAL_NUMBER_TYPE> weights;

        /// KHR_xmp_json_ld: index into the top-level packets array. -1 if not set.
        int64_t khrXmpPacket = -1;

        /// EXT_mesh_manifold: manifold representation of this mesh. nullptr if absent.
        std::shared_ptr<EXTMeshManifold> extMeshManifold = nullptr;

        Mesh(
            const std::string &name,
            const std::vector<Primitive> primitives,
            const std::vector<GLTF_REAL_NUMBER_TYPE> &weights) : GLTFChildOfRoot(name)
        {
            this->primitives = primitives;
            this->weights = weights;
        }
    };

}
