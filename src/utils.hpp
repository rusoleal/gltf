#pragma once

#include <vector>
#include <string>
#include <vector_math/vector2.hpp>
#include <vector_math/vector3.hpp>
#include <vector_math/vector4.hpp>
#include <vector_math/matrix2.hpp>
#include <vector_math/matrix3.hpp>
#include <vector_math/matrix4.hpp>
#include <vector_math/quaternion.hpp>
#include <gltf/gltf.hpp>
#include "json.hpp"

using namespace systems::leal::gltf;
using json = nlohmann::json;


std::vector<std::string> split(const std::string &s, const std::string &delimiter);
systems::leal::vector_math::Vector2<GLTF_REAL_NUMBER_TYPE> *vec2FromGLTF(const nlohmann::json &data);
systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> *vec3FromGLTF(const nlohmann::json &data);
systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> vec3FromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Vector3<GLTF_REAL_NUMBER_TYPE> &defaultValue);
systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> *vec4FromGLTF(const nlohmann::json &data);
systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> vec4FromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Vector4<GLTF_REAL_NUMBER_TYPE> &defaultValue);
systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE> quatFromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Quaternion<GLTF_REAL_NUMBER_TYPE> &defaultValue);
systems::leal::vector_math::Matrix2<GLTF_REAL_NUMBER_TYPE> *mat2FromGLTF(const nlohmann::json &data);
systems::leal::vector_math::Matrix3<GLTF_REAL_NUMBER_TYPE> *mat3FromGLTF(const nlohmann::json &data);
systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> *mat4FromGLTF(const nlohmann::json &data);
systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> mat4FromGLTF(const nlohmann::json &data, const systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> &defaultValue);
void realListFromGLTF(std::vector<GLTF_REAL_NUMBER_TYPE> &output, const nlohmann::json &data);
void intListFromGLTF(std::vector<uint64_t> &output, const nlohmann::json &data);
std::shared_ptr<TextureInfo> textureInfoFromGLTF(const nlohmann::json &data);
AlphaMode alphaModeFromGLTF(const nlohmann::json &data);
FilterMode filterModeFromGLTF(nlohmann::json &node);
std::shared_ptr<std::vector<uint64_t>> uintListFromGLTF(nlohmann::json &node);

bool startsWith(const std::string &str, const std::string &preffix);
bool endsWith(const std::string &str, const std::string &suffix);
