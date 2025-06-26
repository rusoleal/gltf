#define _USE_MATH_DEFINES // windows specific for M_PI constants
#include <cmath>

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

systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> *vec2FromGLTF(const nlohmann::json &data) {
    if (!data.is_array()) {
        return nullptr;
    }
    if (data.size() != 2) {
        return nullptr;
    }
    return new systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE>(data[0], data[1]);
}

systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> *vec3FromGLTF(const nlohmann::json &data) {
    if (!data.is_array()) {
        return nullptr;
    }
    if (data.size() != 3) {
        return nullptr;
    }
    return new systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE>(data[0], data[1], data[2]);
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

systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> *vec4FromGLTF(const nlohmann::json &data) {
    if (!data.is_array()) {
        return nullptr;
    }
    if (data.size() != 4) {
        return nullptr;
    }
    return new systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE>(data[0], data[1], data[2], data[3]);
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

systems::leal::vector_math::Matrix2<GLTF_REAL_NUMBER_TYPE> *mat2FromGLTF(const nlohmann::json &data) {
    if (!data.is_array()) {
        return nullptr;
    }
    if (data.size() != 4) {
        return nullptr;
    }
    auto toReturn = new systems::leal::vector_math::Matrix2<GLTF_REAL_NUMBER_TYPE>();
    for (int a=0; a<4; a++) {
        toReturn->data[a] = data[a];
    }
    return toReturn;
}

systems::leal::vector_math::Matrix3<GLTF_REAL_NUMBER_TYPE> *mat3FromGLTF(const nlohmann::json &data) {
    if (!data.is_array()) {
        return nullptr;
    }
    if (data.size() != 9) {
        return nullptr;
    }
    auto toReturn = new systems::leal::vector_math::Matrix3<GLTF_REAL_NUMBER_TYPE>();
    for (int a=0; a<9; a++) {
        toReturn->data[a] = data[a];
    }
    return toReturn;
}

systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> *mat4FromGLTF(const nlohmann::json &data) {
    if (!data.is_array()) {
        return nullptr;
    }
    if (data.size() != 16) {
        return nullptr;
    }
    auto toReturn = new systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE>();
    for (int a=0; a<16; a++) {
        toReturn->data[a] = data[a];
    }
    return toReturn;
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

std::shared_ptr<TextureInfo> textureInfoFromGLTF(const nlohmann::json &data) {
    if (!data.is_object()) {
        return nullptr;
    }

    return std::make_shared<TextureInfo>(
        data["index"],
        data.value("texCoord",0)
    );
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
