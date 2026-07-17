#pragma once

#include <optional>
#include <string>
#include <vector_math/vector3.hpp>
#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     * Implicit geometric shape types defined by glTF 2.1.
     */
    enum class ShapeType
    {
        box,
        sphere,
        capsule,
        cylinder,
        plane
    };

    /**
     * Base class for implicit geometric shapes introduced in glTF 2.1.
     *
     * Shapes live in a top-level `shapes` array and are referenced by
     * node bounding volumes. They are not rendered directly.
     */
    struct Shape : public GLTFChildOfRoot
    {
        virtual ~Shape() = default;

        Shape(const std::string &name) : GLTFChildOfRoot(name)
        {
        }

        virtual ShapeType getType() const = 0;
    };

    /**
     * Axis-aligned box centered at the origin in local space.
     */
    struct BoxShape : public Shape
    {
        /**
         * Size of the box along the x, y, and z axes.
         */
        systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> size;

        BoxShape(
            const std::string &name,
            const systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> &size) : Shape(name)
        {
            this->size = size;
        }

        ShapeType getType() const override { return ShapeType::box; }
    };

    /**
     * Sphere centered at the origin in local space.
     */
    struct SphereShape : public Shape
    {
        /**
         * Radius of the sphere.
         */
        GLTF_REAL_NUMBER_TYPE radius;

        SphereShape(
            const std::string &name,
            GLTF_REAL_NUMBER_TYPE radius) : Shape(name)
        {
            this->radius = radius;
        }

        ShapeType getType() const override { return ShapeType::sphere; }
    };

    /**
     * Capsule (cylinder with hemispherical ends) centered at the origin
     * and aligned along the Y axis in local space.
     */
    struct CapsuleShape : public Shape
    {
        /**
         * Height of the cylindrical part of the capsule.
         */
        GLTF_REAL_NUMBER_TYPE height;

        /**
         * Radius of the top capping sphere.
         */
        GLTF_REAL_NUMBER_TYPE radiusTop;

        /**
         * Radius of the bottom capping sphere.
         */
        GLTF_REAL_NUMBER_TYPE radiusBottom;

        CapsuleShape(
            const std::string &name,
            GLTF_REAL_NUMBER_TYPE height,
            GLTF_REAL_NUMBER_TYPE radiusTop,
            GLTF_REAL_NUMBER_TYPE radiusBottom) : Shape(name)
        {
            this->height = height;
            this->radiusTop = radiusTop;
            this->radiusBottom = radiusBottom;
        }

        ShapeType getType() const override { return ShapeType::capsule; }
    };

    /**
     * Cylinder centered at the origin and aligned along the Y axis in local
     * space. A cone is a special case where one radius is zero.
     */
    struct CylinderShape : public Shape
    {
        /**
         * Height of the cylinder.
         */
        GLTF_REAL_NUMBER_TYPE height;

        /**
         * Radius of the top face.
         */
        GLTF_REAL_NUMBER_TYPE radiusTop;

        /**
         * Radius of the bottom face.
         */
        GLTF_REAL_NUMBER_TYPE radiusBottom;

        CylinderShape(
            const std::string &name,
            GLTF_REAL_NUMBER_TYPE height,
            GLTF_REAL_NUMBER_TYPE radiusTop,
            GLTF_REAL_NUMBER_TYPE radiusBottom) : Shape(name)
        {
            this->height = height;
            this->radiusTop = radiusTop;
            this->radiusBottom = radiusBottom;
        }

        ShapeType getType() const override { return ShapeType::cylinder; }
    };

    /**
     * Plane centered at the origin in local space, with normal along the +Y
     * axis. By default the plane has infinite extents; finite extents may be
     * supplied via sizeX/sizeZ.
     */
    struct PlaneShape : public Shape
    {
        /**
         * True if the plane is double-sided.
         */
        bool doubleSided;

        /**
         * Optional total size along the local X (tangent) axis.
         */
        std::optional<GLTF_REAL_NUMBER_TYPE> sizeX;

        /**
         * Optional total size along the local Z (bitangent) axis.
         */
        std::optional<GLTF_REAL_NUMBER_TYPE> sizeZ;

        PlaneShape(
            const std::string &name,
            bool doubleSided,
            const std::optional<GLTF_REAL_NUMBER_TYPE> &sizeX,
            const std::optional<GLTF_REAL_NUMBER_TYPE> &sizeZ) : Shape(name)
        {
            this->doubleSided = doubleSided;
            this->sizeX = sizeX;
            this->sizeZ = sizeZ;
        }

        ShapeType getType() const override { return ShapeType::plane; }
    };

}
