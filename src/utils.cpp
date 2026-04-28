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
    // GLTF stores matrices in column-major order, but vector_math::Matrix4
    // uses row-major storage (data[row*4+col]). Transpose during load.
    systems::leal::vector_math::Matrix4<GLTF_REAL_NUMBER_TYPE> toReturn;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            // GLTF column-major: index = col*4 + row
            // vector_math row-major: index = row*4 + col
            toReturn.data[row*4 + col] = data[col*4 + row];
        }
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

// ---------------------------------------------------------------------------
// KHR_texture_procedurals helpers
// ---------------------------------------------------------------------------

static std::shared_ptr<ProceduralValue> proceduralValueFromGLTF(const nlohmann::json &data, const std::string &type)
{
    auto pv = std::make_shared<ProceduralValue>(type);
    if (type == "string" || type == "filename")
    {
        if (data.is_string())
            pv->stringValue = data.get<std::string>();
    }
    else if (type == "boolean")
    {
        if (data.is_boolean())
            pv->boolValue = data.get<bool>();
    }
    else
    {
        // All numeric / tuple / matrix types are represented as arrays of values.
        if (data.is_array())
        {
            pv->values.reserve(data.size());
            for (uint32_t i = 0; i < data.size(); ++i)
            {
                if (data[i].is_number())
                    pv->values.push_back(data[i]);
            }
        }
        else if (data.is_number())
        {
            // Gracefully handle single scalar values even though spec says arrays.
            pv->values.push_back(data);
        }
    }
    return pv;
}

static ProceduralNodeInput proceduralNodeInputFromGLTF(const std::string &name, const nlohmann::json &data)
{
    ProceduralNodeInput input;
    input.name = name;

    if (!data.is_object())
        return input;

    input.type = data.value("type", "");

    if (data.contains("value"))
    {
        input.value = proceduralValueFromGLTF(data["value"], input.type);
    }
    else if (data.contains("node") && data.at("node").is_number_integer())
    {
        input.nodeIndex = data.at("node").get<int64_t>();
        if (data.contains("output") && data.at("output").is_string())
            input.nodeOutput = data.at("output").get<std::string>();
    }
    else if (data.contains("input") && data.at("input").is_string())
    {
        input.graphInput = data.at("input").get<std::string>();
    }

    if (data.contains("xpos") && data.at("xpos").is_number())
        input.xpos = data.at("xpos").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("ypos") && data.at("ypos").is_number())
        input.ypos = data.at("ypos").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("uiname") && data.at("uiname").is_string())
        input.uiname = data.at("uiname").get<std::string>();
    if (data.contains("unittype") && data.at("unittype").is_string())
        input.unittype = data.at("unittype").get<std::string>();
    if (data.contains("doc") && data.at("doc").is_string())
        input.doc = data.at("doc").get<std::string>();
    if (data.contains("uimin") && data.at("uimin").is_number())
        input.uimin = data.at("uimin").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("uimax") && data.at("uimax").is_number())
        input.uimax = data.at("uimax").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("uisoftmin") && data.at("uisoftmin").is_number())
        input.uisoftmin = data.at("uisoftmin").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("uisoftmax") && data.at("uisoftmax").is_number())
        input.uisoftmax = data.at("uisoftmax").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("uistep") && data.at("uistep").is_number())
        input.uistep = data.at("uistep").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("uifolder") && data.at("uifolder").is_string())
        input.uifolder = data.at("uifolder").get<std::string>();

    return input;
}

static ProceduralNodeOutput proceduralNodeOutputFromGLTF(const std::string &name, const nlohmann::json &data)
{
    ProceduralNodeOutput output;
    output.name = name;
    if (data.is_object())
    {
        output.type = data.value("type", "");
        if (data.contains("uiname") && data.at("uiname").is_string())
            output.uiname = data.at("uiname").get<std::string>();
    }
    return output;
}

static ProceduralNode proceduralNodeFromGLTF(const nlohmann::json &data)
{
    ProceduralNode node;
    if (!data.is_object())
        return node;

    node.name = data.value("name", "");
    node.nodetype = data.value("nodetype", "");
    node.type = data.value("type", "");

    auto inputsDef = data["inputs"];
    if (inputsDef.is_object())
    {
        for (auto &element : inputsDef.items())
        {
            node.inputs.push_back(proceduralNodeInputFromGLTF(element.key(), element.value()));
        }
    }

    auto outputsDef = data["outputs"];
    if (outputsDef.is_object())
    {
        for (auto &element : outputsDef.items())
        {
            node.outputs.push_back(proceduralNodeOutputFromGLTF(element.key(), element.value()));
        }
    }

    return node;
}

static ProceduralGraphOutput proceduralGraphOutputFromGLTF(const std::string &name, const nlohmann::json &data)
{
    ProceduralGraphOutput output;
    output.name = name;
    if (data.is_object())
    {
        output.type = data.value("type", "");
        if (data.contains("node") && data.at("node").is_number_integer())
            output.nodeIndex = data.at("node").get<int64_t>();
        if (data.contains("output") && data.at("output").is_string())
            output.nodeOutput = data.at("output").get<std::string>();
        if (data.contains("uiname") && data.at("uiname").is_string())
            output.uiname = data.at("uiname").get<std::string>();
    }
    return output;
}

ProceduralGraph proceduralGraphFromGLTF(const nlohmann::json &data)
{
    ProceduralGraph graph;
    if (!data.is_object())
        return graph;

    graph.name = data.value("name", "");
    graph.nodetype = data.value("nodetype", "");
    graph.type = data.value("type", "");
    graph.nodedef = data.value("nodedef", int64_t(-1));

    if (data.contains("xpos") && data.at("xpos").is_number())
        graph.xpos = data.at("xpos").get<GLTF_REAL_NUMBER_TYPE>();
    if (data.contains("ypos") && data.at("ypos").is_number())
        graph.ypos = data.at("ypos").get<GLTF_REAL_NUMBER_TYPE>();

    auto inputsDef = data["inputs"];
    if (inputsDef.is_object())
    {
        for (auto &element : inputsDef.items())
        {
            graph.inputs.push_back(proceduralNodeInputFromGLTF(element.key(), element.value()));
        }
    }

    auto outputsDef = data["outputs"];
    if (outputsDef.is_object())
    {
        for (auto &element : outputsDef.items())
        {
            graph.outputs.push_back(proceduralGraphOutputFromGLTF(element.key(), element.value()));
        }
    }

    auto nodesDef = data["nodes"];
    if (nodesDef.is_array())
    {
        graph.nodes.reserve(nodesDef.size());
        for (uint32_t i = 0; i < nodesDef.size(); ++i)
        {
            graph.nodes.push_back(proceduralNodeFromGLTF(nodesDef[i]));
        }
    }

    return graph;
}

ProceduralNodeDef proceduralNodeDefFromGLTF(const nlohmann::json &data)
{
    ProceduralNodeDef def;
    if (!data.is_object())
        return def;

    def.name = data.value("name", "");
    def.nodetype = data.value("nodetype", "");
    def.node = data.value("node", "");
    def.nodegroup = data.value("nodegroup", "");
    def.version = data.value("version", "");
    def.defaultVersion = data.value("defaultVersion", false);

    auto inputsDef = data["inputs"];
    if (inputsDef.is_object())
    {
        for (auto &element : inputsDef.items())
        {
            def.inputs.push_back(proceduralNodeInputFromGLTF(element.key(), element.value()));
        }
    }

    auto outputsDef = data["outputs"];
    if (outputsDef.is_object())
    {
        for (auto &element : outputsDef.items())
        {
            def.outputs.push_back(proceduralNodeOutputFromGLTF(element.key(), element.value()));
        }
    }

    return def;
}

std::shared_ptr<KHRTextureProceduralsTextureInfo> khrTextureProceduralsTextureInfoFromGLTF(const nlohmann::json &data)
{
    if (!data.is_object())
        return nullptr;

    if (!data.contains("index") || !data.at("index").is_number_integer())
        return nullptr;

    int64_t index = data.at("index").get<int64_t>();
    std::string output;
    if (data.contains("output") && data.at("output").is_string())
        output = data.at("output").get<std::string>();

    return std::make_shared<KHRTextureProceduralsTextureInfo>(index, output);
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
    if (data.contains("extensions") && data.at("extensions").is_object() &&
        data.at("extensions").contains("KHR_texture_procedurals")) {
        ti->khrTextureProcedurals = khrTextureProceduralsTextureInfoFromGLTF(data.at("extensions").at("KHR_texture_procedurals"));
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
