#pragma once

#include <vector>

namespace systems::leal::gltf {

    struct RuntimeInfo {
        std::vector<bool> buffers;
        std::vector<bool> images;

        RuntimeInfo(uint64_t buffersSize, uint64_t imagesSize) {
            buffers = std::vector<bool>(buffersSize,false);
            images = std::vector<bool>(imagesSize,false);
        }
    };
}