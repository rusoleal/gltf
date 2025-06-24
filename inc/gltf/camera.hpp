#pragma once

#include <gltf/gltf_child_of_root.hpp>

namespace systems::leal::gltf
{

    /**
     */
    struct Camera : public GLTFChildOfRoot
    {

        Camera(const std::string &name) : GLTFChildOfRoot(name)
        {
        }
    };

    /**
     * An orthographic camera containing properties to create an orthographic
     * projection matrix.
     */
    struct OrthographicCamera : Camera
    {

        /**
         * The floating-point horizontal magnification of the view. This value
         * MUST NOT be equal to zero. This value SHOULD NOT be negative.
         */
        GLTF_REAL_NUMBER_TYPE xMag;

        /**
         * The floating-point vertical magnification of the view. This value MUST NOT
         * be equal to zero. This value SHOULD NOT be negative.
         */
        GLTF_REAL_NUMBER_TYPE yMag;

        /**
         * The floating-point distance to the far clipping plane. This value MUST NOT
         * be equal to zero. zfar MUST be greater than [zNear].
         */
        GLTF_REAL_NUMBER_TYPE zFar;

        /**
         * The floating-point distance to the near clipping plane.
         */
        GLTF_REAL_NUMBER_TYPE zNear;

        OrthographicCamera(
            const std::string &name,
            GLTF_REAL_NUMBER_TYPE xMag, 
            GLTF_REAL_NUMBER_TYPE yMag, 
            GLTF_REAL_NUMBER_TYPE zFar, 
            GLTF_REAL_NUMBER_TYPE zNear):Camera(name)
        {
            this->xMag = xMag;
            this->yMag = yMag;
            this->zFar = zFar;
            this->zNear = zNear;
        }
    };

    /**
     * A perspective camera containing properties to create a perspective
     * projection matrix.
     */
    struct PerspectiveCamera : Camera
    {

        /**
         * The floating-point aspect ratio of the field of view. When undefined, the
         * aspect ratio of the rendering viewport MUST be used.
         */
        GLTF_REAL_NUMBER_TYPE *aspectRatio;

        /**
         * The floating-point vertical field of view in radians. This value SHOULD be
         * less than [pi].
         */
        GLTF_REAL_NUMBER_TYPE yFov;

        /**
         * The floating-point distance to the far clipping plane. When defined, zfar
         * MUST be greater than [zNear]. If zfar is undefined, client implementations
         * SHOULD use infinite projection matrix.
         */
        GLTF_REAL_NUMBER_TYPE *zFar;

        /**
         * The floating-point distance to the near clipping plane.
         */
        GLTF_REAL_NUMBER_TYPE zNear;

        PerspectiveCamera(
            const std::string &name,
            GLTF_REAL_NUMBER_TYPE *aspectRatio, 
            GLTF_REAL_NUMBER_TYPE yFov, 
            GLTF_REAL_NUMBER_TYPE *zFar, 
            GLTF_REAL_NUMBER_TYPE zNear):Camera(name)
        {
            this->aspectRatio = aspectRatio;
            this->yFov = yFov;
            this->zFar = zFar;
            this->zNear = zNear;
        }

        ~PerspectiveCamera()
        {
            if (aspectRatio != nullptr)
            {
                delete aspectRatio;
            }
            if (zFar != nullptr)
            {
                delete zFar;
            }
        }
    };
}