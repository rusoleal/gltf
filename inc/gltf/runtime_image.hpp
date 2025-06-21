#pragma once

namespace systems::leal::gltf {

    enum class RuntimeImageFormat {
        uint8,
        float32
    };

    struct RuntimeImage {

        uint32_t width;
        uint32_t height;
        uint32_t components;
        RuntimeImageFormat format;
        uint8_t *data;

        RuntimeImage(
            uint32_t width,
            uint32_t height,
            uint32_t components,
            RuntimeImageFormat format,
            uint8_t *data) {

            this->width = width;
            this->height = height;
            this->components = components;
            this->format = format;
            this->data = data;                
        }

        ~RuntimeImage() {
            if (data != nullptr) {
                delete data;
            }
        }

    };
}