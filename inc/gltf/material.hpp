#pragma once

#include <limits>
#include <memory>
#include <vector_math/vector4.hpp>
#include <gltf/gltf_child_of_root.hpp>
#include <gltf/extensions/khr_texture_transform.hpp>

namespace systems::leal::gltf
{

    /**
     * The material's alpha rendering mode enumeration specifying
     * the interpretation of the alpha value of the base color.
     */
    enum AlphaMode
    {

        /**
         * The alpha value is ignored, and the rendered output is fully opaque.
         */
        opaque,

        /**
         * The rendered output is either fully opaque or fully transparent depending
         * on the alpha value and the specified alphaCutoff value; the exact
         * appearance of the edges MAY be subject to implementation-specific
         * techniques such as "Alpha-to-Coverage".
         */
        mask,

        /**
         * The alpha value is used to composite the source and destination areas.
         * The rendered output is combined with the background using the normal
         * painting operation (i.e. the Porter and Duff over operator).
         */
        blend,
    };

    /// Base class for texture base information.
    struct TextureInfo
    {
        /// The index of the [Texture].
        uint64_t index;

        /// This integer value is used to construct a string in the format
        /// TEXCOORD_&lt;set index&gt; which is a reference to a key in
        /// mesh.primitives.attributes (e.g. a value of 0 corresponds to TEXCOORD_0).
        /// A mesh primitive MUST have the corresponding texture coordinate attributes
        /// for the material to be applicable to it.
        uint64_t texCoord;

        /// KHR_texture_transform extension data. nullptr if not present.
        std::shared_ptr<KHRTextureTransform> khrTextureTransform = nullptr;

        TextureInfo(uint64_t index, uint64_t texCoord)
        {
            this->index = index;
            this->texCoord = texCoord;
        }
    };

    /// A set of parameter values that are used to define the metallic-roughness
    /// material model from Physically Based Rendering (PBR) methodology.
    struct PBRMetallicRoughness
    {
        /// The factors for the base color of the material. This value defines linear
        /// multipliers for the sampled texels of the base color texture.
        systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> baseColorFactor;

        /// The base color texture. The first three components (RGB) MUST be encoded
        /// with the sRGB transfer function. They specify the base color of the
        /// material. If the fourth component (A) is present, it represents the linear
        /// alpha coverage of the material. Otherwise, the alpha coverage is equal
        /// to 1.0. The [Material.alphaMode] property specifies how alpha is
        /// interpreted. The stored texels MUST NOT be premultiplied. When undefined,
        /// the texture MUST be sampled as having 1.0 in all components.
        std::shared_ptr<TextureInfo> baseColorTexture;

        /// The factor for the metalness of the material. This value defines a linear
        /// multiplier for the sampled metalness values of the metallic-roughness
        /// texture.
        GLTF_REAL_NUMBER_TYPE metallicFactor;

        /// The factor for the roughness of the material. This value defines a linear
        /// multiplier for the sampled roughness values of the metallic-roughness
        /// texture.
        GLTF_REAL_NUMBER_TYPE roughnessFactor;

        /// The metallic-roughness texture. The metalness values are sampled from the
        /// B channel. The roughness values are sampled from the G channel. These
        /// values MUST be encoded with a linear transfer function. If other channels
        /// are present (R or A), they MUST be ignored for metallic-roughness
        /// calculations. When undefined, the texture MUST be sampled as having 1.0
        /// in G and B components.
        std::shared_ptr<TextureInfo> metallicRoughnessTexture;

        PBRMetallicRoughness(
            const systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> &baseColorFactor,
            std::shared_ptr<TextureInfo> baseColorTexture,
            GLTF_REAL_NUMBER_TYPE metallicFactor,
            GLTF_REAL_NUMBER_TYPE roughnessFactor,
            std::shared_ptr<TextureInfo> metallicRoughnessTexture)
        {
            this->baseColorFactor = baseColorFactor;
            this->baseColorTexture = baseColorTexture;
            this->metallicFactor = metallicFactor;
            this->roughnessFactor = roughnessFactor;
            this->metallicRoughnessTexture = metallicRoughnessTexture;
        }
    };

    /// The tangent space normal texture.
    ///
    /// The texture encodes RGB components with linear transfer function. Each texel
    /// represents the XYZ components of a normal vector in tangent space.
    ///
    /// The normal vectors use the convention +X is right and +Y is up. +Z points
    /// toward the viewer. If a fourth component (A) is present, it MUST be ignored.
    struct NormalTextureInfo : TextureInfo
    {
        /// The scalar parameter applied to each normal vector of the texture. This
        /// value scales the normal vector in X and Y directions using the formula:
        /// scaledNormal = normalize&lt;sampled normal texture value&gt; * 2.0 - 1.0) *
        /// vec3(&lt;normal scale&gt;, &lt;normal scale&gt;, 1.0).
        GLTF_REAL_NUMBER_TYPE scale;

        NormalTextureInfo(uint64_t index, uint64_t texCoord, GLTF_REAL_NUMBER_TYPE scale)
            : TextureInfo(index, texCoord)
        {
            this->scale = scale;
        }
    };

    /// The occlusion texture.
    ///
    /// The occlusion values are linearly sampled from the R channel.
    ///
    /// Higher values indicate areas that receive full indirect lighting and lower
    /// values indicate no indirect lighting. If other channels are present (GBA),
    /// they MUST be ignored for occlusion calculations.
    struct OcclusionTextureInfo : TextureInfo
    {

        /// A scalar parameter controlling the amount of occlusion applied. A value
        /// of 0.0 means no occlusion. A value of 1.0 means full occlusion. This value
        /// affects the final occlusion value as:
        /// 1.0 + strength * (&lt;sampled occlusion texture value&gt; - 1.0).
        GLTF_REAL_NUMBER_TYPE strength;

        OcclusionTextureInfo(uint64_t index, uint64_t texCoord, GLTF_REAL_NUMBER_TYPE strength)
            : TextureInfo(index, texCoord)
        {
            this->strength = strength;
        }
    };

    // -------------------------------------------------------------------------
    // KHR material extension structs
    // Defined after NormalTextureInfo / OcclusionTextureInfo so they can
    // reference those types.
    // -------------------------------------------------------------------------

    /**
     * KHR_materials_transmission extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_transmission
     */
    struct KHRMaterialsTransmission
    {
        /// The base percentage of light that is transmitted through the surface. Default 0.
        GLTF_REAL_NUMBER_TYPE transmissionFactor;

        /// A texture that defines the transmission percentage of the surface.
        std::shared_ptr<TextureInfo> transmissionTexture;

        KHRMaterialsTransmission(
            GLTF_REAL_NUMBER_TYPE transmissionFactor,
            std::shared_ptr<TextureInfo> transmissionTexture)
            : transmissionFactor(transmissionFactor), transmissionTexture(transmissionTexture) {}
    };

    /**
     * KHR_materials_clearcoat extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_clearcoat
     */
    struct KHRMaterialsClearcoat
    {
        /// The clearcoat layer intensity. Default 0.
        GLTF_REAL_NUMBER_TYPE clearcoatFactor;

        std::shared_ptr<TextureInfo> clearcoatTexture;

        /// The clearcoat layer roughness. Default 0.
        GLTF_REAL_NUMBER_TYPE clearcoatRoughnessFactor;

        std::shared_ptr<TextureInfo> clearcoatRoughnessTexture;

        std::shared_ptr<NormalTextureInfo> clearcoatNormalTexture;

        KHRMaterialsClearcoat(
            GLTF_REAL_NUMBER_TYPE clearcoatFactor,
            std::shared_ptr<TextureInfo> clearcoatTexture,
            GLTF_REAL_NUMBER_TYPE clearcoatRoughnessFactor,
            std::shared_ptr<TextureInfo> clearcoatRoughnessTexture,
            std::shared_ptr<NormalTextureInfo> clearcoatNormalTexture)
            : clearcoatFactor(clearcoatFactor),
              clearcoatTexture(clearcoatTexture),
              clearcoatRoughnessFactor(clearcoatRoughnessFactor),
              clearcoatRoughnessTexture(clearcoatRoughnessTexture),
              clearcoatNormalTexture(clearcoatNormalTexture) {}
    };

    /**
     * KHR_materials_sheen extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_sheen
     */
    struct KHRMaterialsSheen
    {
        /// The sheen color in linear space. Default [0, 0, 0].
        systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> sheenColorFactor;

        std::shared_ptr<TextureInfo> sheenColorTexture;

        /// The sheen roughness. Default 0.
        GLTF_REAL_NUMBER_TYPE sheenRoughnessFactor;

        std::shared_ptr<TextureInfo> sheenRoughnessTexture;

        KHRMaterialsSheen(
            const systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> &sheenColorFactor,
            std::shared_ptr<TextureInfo> sheenColorTexture,
            GLTF_REAL_NUMBER_TYPE sheenRoughnessFactor,
            std::shared_ptr<TextureInfo> sheenRoughnessTexture)
            : sheenColorFactor(sheenColorFactor),
              sheenColorTexture(sheenColorTexture),
              sheenRoughnessFactor(sheenRoughnessFactor),
              sheenRoughnessTexture(sheenRoughnessTexture) {}
    };

    /**
     * KHR_materials_specular extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_specular
     */
    struct KHRMaterialsSpecular
    {
        /// Strength of the specular reflection. Default 1.
        GLTF_REAL_NUMBER_TYPE specularFactor;

        std::shared_ptr<TextureInfo> specularTexture;

        /// The F0 color of the specular reflection in linear space. Default [1, 1, 1].
        systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> specularColorFactor;

        std::shared_ptr<TextureInfo> specularColorTexture;

        KHRMaterialsSpecular(
            GLTF_REAL_NUMBER_TYPE specularFactor,
            std::shared_ptr<TextureInfo> specularTexture,
            const systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> &specularColorFactor,
            std::shared_ptr<TextureInfo> specularColorTexture)
            : specularFactor(specularFactor),
              specularTexture(specularTexture),
              specularColorFactor(specularColorFactor),
              specularColorTexture(specularColorTexture) {}
    };

    /**
     * KHR_materials_volume extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_volume
     */
    struct KHRMaterialsVolume
    {
        /// The thickness of the volume beneath the surface in object space. Default 0.
        GLTF_REAL_NUMBER_TYPE thicknessFactor;

        std::shared_ptr<TextureInfo> thicknessTexture;

        /// Mean free path for absorption, in world units. Default +Infinity.
        GLTF_REAL_NUMBER_TYPE attenuationDistance;

        /// Color that white light turns into due to absorption. Default [1, 1, 1].
        systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> attenuationColor;

        KHRMaterialsVolume(
            GLTF_REAL_NUMBER_TYPE thicknessFactor,
            std::shared_ptr<TextureInfo> thicknessTexture,
            GLTF_REAL_NUMBER_TYPE attenuationDistance,
            const systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> &attenuationColor)
            : thicknessFactor(thicknessFactor),
              thicknessTexture(thicknessTexture),
              attenuationDistance(attenuationDistance),
              attenuationColor(attenuationColor) {}
    };

    /**
     * KHR_materials_iridescence extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_iridescence
     */
    struct KHRMaterialsIridescence
    {
        /// Strength of the thin-film layer. Default 0.
        GLTF_REAL_NUMBER_TYPE iridescenceFactor;

        std::shared_ptr<TextureInfo> iridescenceTexture;

        /// Index of refraction of the thin-film layer. Default 1.3.
        GLTF_REAL_NUMBER_TYPE iridescenceIor;

        /// Minimum thickness of the thin-film layer in nm. Default 100.
        GLTF_REAL_NUMBER_TYPE iridescenceThicknessMinimum;

        /// Maximum thickness of the thin-film layer in nm. Default 400.
        GLTF_REAL_NUMBER_TYPE iridescenceThicknessMaximum;

        std::shared_ptr<TextureInfo> iridescenceThicknessTexture;

        KHRMaterialsIridescence(
            GLTF_REAL_NUMBER_TYPE iridescenceFactor,
            std::shared_ptr<TextureInfo> iridescenceTexture,
            GLTF_REAL_NUMBER_TYPE iridescenceIor,
            GLTF_REAL_NUMBER_TYPE iridescenceThicknessMinimum,
            GLTF_REAL_NUMBER_TYPE iridescenceThicknessMaximum,
            std::shared_ptr<TextureInfo> iridescenceThicknessTexture)
            : iridescenceFactor(iridescenceFactor),
              iridescenceTexture(iridescenceTexture),
              iridescenceIor(iridescenceIor),
              iridescenceThicknessMinimum(iridescenceThicknessMinimum),
              iridescenceThicknessMaximum(iridescenceThicknessMaximum),
              iridescenceThicknessTexture(iridescenceThicknessTexture) {}
    };

    /**
     * KHR_materials_anisotropy extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_anisotropy
     */
    struct KHRMaterialsAnisotropy
    {
        /// Anisotropy strength. Default 0.
        GLTF_REAL_NUMBER_TYPE anisotropyStrength;

        /// Rotation of the anisotropy in radians, counter-clockwise from the tangent. Default 0.
        GLTF_REAL_NUMBER_TYPE anisotropyRotation;

        std::shared_ptr<TextureInfo> anisotropyTexture;

        KHRMaterialsAnisotropy(
            GLTF_REAL_NUMBER_TYPE anisotropyStrength,
            GLTF_REAL_NUMBER_TYPE anisotropyRotation,
            std::shared_ptr<TextureInfo> anisotropyTexture)
            : anisotropyStrength(anisotropyStrength),
              anisotropyRotation(anisotropyRotation),
              anisotropyTexture(anisotropyTexture) {}
    };

    /**
     * The material appearance of a [Primitive].
     */
    struct Material : public GLTFChildOfRoot
    {

        /// A set of parameter values that are used to define the metallic-roughness
        /// material model from Physically Based Rendering (PBR) methodology.
        ///
        /// When undefined, all the default values of pbrMetallicRoughness MUST apply.
        std::shared_ptr<PBRMetallicRoughness> pbrMetallicRoughness;

        /// The tangent space normal texture. The texture encodes RGB components with
        /// linear transfer function. Each texel represents the XYZ components of a
        /// normal vector in tangent space. The normal vectors use the convention
        /// +X is right and +Y is up. +Z points toward the viewer. If a fourth
        /// component (A) is present, it MUST be ignored. When undefined, the material
        /// does not have a tangent space normal texture.
        std::shared_ptr<NormalTextureInfo> normalTexture;

        /// The occlusion texture. The occlusion values are linearly sampled from the
        /// R channel. Higher values indicate areas that receive full indirect
        /// lighting and lower values indicate no indirect lighting. If other channels
        /// are present (GBA), they MUST be ignored for occlusion calculations. When
        /// undefined, the material does not have an occlusion texture.
        std::shared_ptr<OcclusionTextureInfo> occlusionTexture;

        /// The emissive texture. It controls the color and intensity of the light
        /// being emitted by the material. This texture contains RGB components
        /// encoded with the sRGB transfer function. If a fourth component (A) is
        /// present, it MUST be ignored. When undefined, the texture MUST be sampled
        /// as having 1.0 in RGB components.
        std::shared_ptr<TextureInfo> emissiveTexture;

        /// The factors for the emissive color of the material. This value defines
        /// linear multipliers for the sampled texels of the emissive texture.
        systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> emissiveFactor;

        /// The material's alpha rendering mode enumeration specifying the
        /// interpretation of the alpha value of the base color.
        AlphaMode alphaMode;

        /// Specifies the cutoff threshold when in [AlphaMode.mask]. If the alpha
        /// value is greater than or equal to this value then it is rendered as fully
        /// opaque, otherwise, it is rendered as fully transparent. A value greater
        /// than 1.0 will render the entire material as fully transparent. This value
        /// MUST be ignored for other alpha modes. When alphaMode is not defined, this
        /// value MUST NOT be defined.
        GLTF_REAL_NUMBER_TYPE alphaCutoff;

        /// Specifies whether the material is double sided. When this value is false,
        /// back-face culling is enabled. When this value is true, back-face culling
        /// is disabled and double-sided lighting is enabled. The back-face MUST have
        /// its normals reversed before the lighting equation is evaluated.
        bool doubleSided;

        // KHR material extension structs — nullptr when the extension is absent.
        std::shared_ptr<KHRMaterialsTransmission> khrMaterialsTransmission = nullptr;
        std::shared_ptr<KHRMaterialsClearcoat>    khrMaterialsClearcoat    = nullptr;
        std::shared_ptr<KHRMaterialsSheen>        khrMaterialsSheen        = nullptr;
        std::shared_ptr<KHRMaterialsSpecular>     khrMaterialsSpecular     = nullptr;
        std::shared_ptr<KHRMaterialsVolume>       khrMaterialsVolume       = nullptr;
        std::shared_ptr<KHRMaterialsIridescence>  khrMaterialsIridescence  = nullptr;
        std::shared_ptr<KHRMaterialsAnisotropy>   khrMaterialsAnisotropy   = nullptr;

        /// KHR_materials_unlit: when true, the material should be rendered without
        /// lighting (unlit shading model).
        bool khrMaterialsUnlit;

        /// KHR_materials_emissive_strength: multiplier applied to the emissiveFactor.
        /// Default value is 1.0.
        GLTF_REAL_NUMBER_TYPE khrMaterialsEmissiveStrength;

        /// KHR_materials_ior: index of refraction of the material.
        /// Default value is 1.5.
        GLTF_REAL_NUMBER_TYPE khrMaterialsIor;

        /// KHR_materials_dispersion: strength of dispersion effect.
        /// Default value is 0.0 (no dispersion).
        GLTF_REAL_NUMBER_TYPE khrMaterialsDispersion;

        /// KHR_xmp_json_ld: index into the top-level packets array. -1 if not set.
        int64_t khrXmpPacket = -1;

        Material(
            const std::string &name,
            std::shared_ptr<PBRMetallicRoughness> pbrMetallicRoughness,
            std::shared_ptr<NormalTextureInfo> normalTexture,
            std::shared_ptr<OcclusionTextureInfo> occlusionTexture,
            std::shared_ptr<TextureInfo> emissiveTexture,
            systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> emissiveFactor,
            AlphaMode alphaMode,
            GLTF_REAL_NUMBER_TYPE alphaCutoff,
            bool doubleSided,
            bool khrMaterialsUnlit,
            GLTF_REAL_NUMBER_TYPE khrMaterialsEmissiveStrength,
            GLTF_REAL_NUMBER_TYPE khrMaterialsIor,
            GLTF_REAL_NUMBER_TYPE khrMaterialsDispersion) : GLTFChildOfRoot(name)
        {
            this->pbrMetallicRoughness = pbrMetallicRoughness;
            this->normalTexture = normalTexture;
            this->occlusionTexture = occlusionTexture;
            this->emissiveTexture = emissiveTexture;
            this->emissiveFactor = emissiveFactor;
            this->alphaMode = alphaMode;
            this->alphaCutoff = alphaCutoff;
            this->doubleSided = doubleSided;
            this->khrMaterialsUnlit = khrMaterialsUnlit;
            this->khrMaterialsEmissiveStrength = khrMaterialsEmissiveStrength;
            this->khrMaterialsIor = khrMaterialsIor;
            this->khrMaterialsDispersion = khrMaterialsDispersion;
        }
    };
}
