#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace systems::leal::gltf
{

    /**
     * KHR_texture_procedurals extension data.
     * https://github.com/KhronosGroup/glTF/tree/KHR_texture_procedurals/extensions/2.0/Khronos/KHR_texture_procedurals
     */

    struct ProceduralValue
    {
        std::string type;
        std::vector<GLTF_REAL_NUMBER_TYPE> values;
        std::string stringValue;
        bool boolValue = false;

        ProceduralValue() = default;
        explicit ProceduralValue(const std::string &type) : type(type) {}
    };

    struct ProceduralNodeInput
    {
        std::string name;
        std::string type;
        std::shared_ptr<ProceduralValue> value;
        int64_t nodeIndex = -1;
        std::string nodeOutput;
        std::string graphInput;
        GLTF_REAL_NUMBER_TYPE xpos = 0;
        GLTF_REAL_NUMBER_TYPE ypos = 0;
        std::string uiname;
        std::string unittype;
        std::string doc;
        GLTF_REAL_NUMBER_TYPE uimin = 0;
        GLTF_REAL_NUMBER_TYPE uimax = 0;
        GLTF_REAL_NUMBER_TYPE uisoftmin = 0;
        GLTF_REAL_NUMBER_TYPE uisoftmax = 0;
        GLTF_REAL_NUMBER_TYPE uistep = 0;
        std::string uifolder;
    };

    struct ProceduralNodeOutput
    {
        std::string name;
        std::string type;
        std::string uiname;
    };

    struct ProceduralNode
    {
        std::string name;
        std::string nodetype;
        std::string type;
        std::vector<ProceduralNodeInput> inputs;
        std::vector<ProceduralNodeOutput> outputs;
    };

    struct ProceduralGraphOutput
    {
        std::string name;
        std::string type;
        int64_t nodeIndex = -1;
        std::string nodeOutput;
        std::string uiname;
    };

    struct ProceduralGraph
    {
        std::string name;
        std::string nodetype;
        std::string type;
        std::vector<ProceduralNodeInput> inputs;
        std::vector<ProceduralGraphOutput> outputs;
        std::vector<ProceduralNode> nodes;
        int64_t nodedef = -1;
        GLTF_REAL_NUMBER_TYPE xpos = 0;
        GLTF_REAL_NUMBER_TYPE ypos = 0;
    };

    struct ProceduralNodeDef
    {
        std::string name;
        std::string nodetype;
        std::string node;
        std::string nodegroup;
        std::string version;
        bool defaultVersion = false;
        std::vector<ProceduralNodeInput> inputs;
        std::vector<ProceduralNodeOutput> outputs;
    };

    struct KHRTextureProcedurals
    {
        std::vector<ProceduralGraph> procedurals;
        std::vector<ProceduralNodeDef> proceduralDefinitions;
    };

    struct KHRTextureProceduralsTextureInfo
    {
        int64_t index;
        std::string output;

        KHRTextureProceduralsTextureInfo(int64_t index, const std::string &output)
            : index(index), output(output) {}
    };

}
