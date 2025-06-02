#pragma once

namespace systems::leal::gltf {

    /**
     */
    struct Camera {
    };

    /**
     * An orthographic camera containing properties to create an orthographic
     * projection matrix.
     */
    struct OrthographicCamera: Camera {

        /**
         * The floating-point horizontal magnification of the view. This value
         * MUST NOT be equal to zero. This value SHOULD NOT be negative.
         */
        double xMag;

        /**
         * The floating-point vertical magnification of the view. This value MUST NOT
         * be equal to zero. This value SHOULD NOT be negative.
         */
        double yMag;

        /**
         * The floating-point distance to the far clipping plane. This value MUST NOT
         * be equal to zero. zfar MUST be greater than [zNear].
         */
        double zFar;

        /**
         * The floating-point distance to the near clipping plane.
         */
        double zNear;

        OrthographicCamera(double xMag, double yMag, double zFar, double zNear) {
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
    struct PerspectiveCamera: Camera {

        /**
         * The floating-point aspect ratio of the field of view. When undefined, the
         * aspect ratio of the rendering viewport MUST be used.
         */
        double *aspectRatio;

        /**
         * The floating-point vertical field of view in radians. This value SHOULD be
         * less than [pi].
         */
        double yFov;

        /**
         * The floating-point distance to the far clipping plane. When defined, zfar
         * MUST be greater than [zNear]. If zfar is undefined, client implementations
         * SHOULD use infinite projection matrix.
         */
        double *zFar;

        /**
         * The floating-point distance to the near clipping plane.
         */
        double zNear;

        PerspectiveCamera(double *aspectRatio, double yFov, double *zFar, double zNear) {
            this->aspectRatio = aspectRatio;
            this->yFov = yFov;
            this->zFar = zFar;
            this->zNear = zNear;
        }

        ~PerspectiveCamera() {
            if (aspectRatio != nullptr) {
                delete aspectRatio;
            }
            if (zFar != nullptr) {
                delete zFar;
            }
        }

    };
}