#define _USE_MATH_DEFINES // windows specific for M_PI constants
#include <cmath>
#include <stdexcept>

#include "utils.hpp"

FilterMode filterModeFromGLTF(nlohmann::json &node) {
    if (node.is_number()) {
        return (FilterMode)(uint64_t)node;
    }
    return FilterMode::fmUnknown;
}

std::shared_ptr<std::vector<uint64_t>> uintListFromGLTF(nlohmann::json &node) {
    if (node.is_array()) {
        auto toReturn = std::make_shared<std::vector<uint64_t>>();
        toReturn->reserve(node.size());
        for (uint64_t a=0; a<node.size(); a++) {
            toReturn->emplace_back(node[a]);
        }
        return toReturn;
    }
    return nullptr;
}

std::vector<std::string> split(const std::string &s, const std::string &delimiter) {

    if (delimiter.empty()) return {};

    std::string copy(s);
    size_t pos = 0;
    std::string token;
    std::vector<std::string> toReturn;
    while ((pos = copy.find(delimiter)) != std::string::npos) {
        token = copy.substr(0, pos);
        toReturn.push_back(token);
        copy.erase(0, pos + delimiter.length());
    }
    
    if (!copy.empty())
        toReturn.push_back(copy);

    return toReturn;
}

bool startsWith(const std::string &str, const std::string &preffix) {
    return str.rfind(preffix,0) == 0;
}

bool endsWith(const std::string &str, const std::string &suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> vec3FromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> &defaultValue) {
    if (!data.is_array()) {
        return defaultValue;
    }
    if (data.size() != 3) {
        return defaultValue;
    }
    return  { data[0], data[1], data[2] };
}

systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> vec4FromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> &defaultValue) {
    if (!data.is_array()) {
        return defaultValue;
    }
    if (data.size() != 4) {
        return defaultValue;
    }
    return systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE>(data[0], data[1], data[2], data[3]);
}


systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE> quatFromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE> &defaultValue) {
    if (!data.is_array()) {
        return defaultValue;
    }
    if (data.size() != 4) {
        return defaultValue;
    }
    return systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE>(data[0], data[1], data[2], data[3]);
}

systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> mat4FromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> &defaultValue) {
    if (!data.is_array()) {
        return defaultValue;
    }
    if (data.size() != 16) {
        return defaultValue;
    }
    systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> toReturn;
    for (int a=0; a<16; a++) {
        toReturn.data[a] = data[a];
    }
    return toReturn;
}


void realListFromGLTF(std::vector<GLTF_REAL_NUMBER_TYPE> &output, const nlohmann::json &data) {
    if (!data.is_array()) {
        return;
    }

    for (int a=0; a<data.size(); a++) {
        output.push_back(data[a]);
    }
}

void intListFromGLTF(std::vector<uint64_t> &output, const nlohmann::json &data) {
    if (!data.is_array()) {
        return;
    }

    for (int a=0; a<data.size(); a++) {
        output.push_back(data[a]);
    }
}

std::shared_ptr<KHRTextureTransform> khrTextureTransformFromGLTF(const nlohmann::json &data) {
    if (!data.is_object()) {
        return nullptr;
    }

    systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> offset(0.0, 0.0);
    if (data.contains("offset") && data.at("offset").is_array() && data.at("offset").size() >= 2) {
        offset = { data.at("offset")[0], data.at("offset")[1] };
    }

    GLTF_REAL_NUMBER_TYPE rotation = data.value("rotation", 0.0);

    systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> scale(1.0, 1.0);
    if (data.contains("scale") && data.at("scale").is_array() && data.at("scale").size() >= 2) {
        scale = { data.at("scale")[0], data.at("scale")[1] };
    }

    int64_t texCoord = data.value("texCoord", int64_t(-1));

    return std::make_shared<KHRTextureTransform>(offset, rotation, scale, texCoord);
}

std::shared_ptr<TextureInfo> textureInfoFromGLTF(const nlohmann::json &data) {
    if (!data.is_object()) {
        return nullptr;
    }

    if (!data.contains("index") || !data.at("index").is_number())
        throw std::invalid_argument("TextureInfo missing required 'index' field");
    auto ti = std::make_shared<TextureInfo>(data.at("index"), data.value("texCoord", 0));
    if (data.contains("extensions") && data.at("extensions").is_object() &&
        data.at("extensions").contains("KHR_texture_transform")) {
        ti->khrTextureTransform = khrTextureTransformFromGLTF(data.at("extensions").at("KHR_texture_transform"));
    }
    return ti;
}

AlphaMode alphaModeFromGLTF(const nlohmann::json &data) {

    if (!data.is_string()) {
        return AlphaMode::opaque;
    }

    std::string value = data;
    if (value == "OPAQUE") return AlphaMode::opaque;
    if (value == "MASK") return AlphaMode::mask;
    if (value == "BLEND") return AlphaMode::blend;
    return AlphaMode::opaque;
}
