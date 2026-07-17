#pragma once

#include <string>

namespace systems::leal::gltf
{
    /**
     * KHR_gaussian_splatting primitive extension data.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_gaussian_splatting
     */
    struct KHRGaussianSplatting
    {
        /// Kernel used to generate the Gaussians (e.g. "ellipse").
        std::string kernel;

        /// Color space of the reconstructed color values.
        std::string colorSpace;

        /// Projection method for rendering the Gaussians.
        std::string projection = "perspective";

        /// Sorting method for rendering the Gaussians.
        std::string sortingMethod = "cameraDistance";

        KHRGaussianSplatting(
            const std::string &kernel,
            const std::string &colorSpace,
            const std::string &projection,
            const std::string &sortingMethod)
            : kernel(kernel), colorSpace(colorSpace), projection(projection), sortingMethod(sortingMethod) {}
    };
}
