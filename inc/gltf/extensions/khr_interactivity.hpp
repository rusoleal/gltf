#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace systems::leal::gltf
{

    /**
     * KHR_interactivity extension data: behavior graphs stored at the root.
     * https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_interactivity
     *
     * The library parses and validates the graph structure (types, variables,
     * custom events, operation declarations, nodes and their sockets).
     * Executing the graph is left to the caller.
     *
     * An invalid extension or graph never fails the glTF load: the spec says
     * the asset is then treated as having no interactivity (or that graph is
     * not used). Check KHRInteractivity::valid and InteractivityGraph::valid.
     */

    /// Value type signatures defined by KHR_interactivity.
    enum class InteractivityTypeSignature
    {
        Bool,
        Int,
        Float,
        Float2,
        Float3,
        Float4,
        Float2x2,
        Float3x3,
        Float4x4,
        Ref,
        Custom
    };

    /// One element of a literal value array.
    struct InteractivityLiteral
    {
        enum class Kind
        {
            Null,
            Bool,
            Integer,
            Number,
            String
        };

        Kind kind = Kind::Null;
        bool boolValue = false;
        /// Exact value when kind == Integer (also mirrored in numberValue).
        int64_t intValue = 0;
        /// Numeric value when kind == Integer or Number; 0/1 for Bool.
        GLTF_REAL_NUMBER_TYPE numberValue = 0;
        std::string stringValue;
    };

    /// A literal value. glTF always encodes values as JSON arrays (scalars are arrays of length 1).
    using InteractivityValue = std::vector<InteractivityLiteral>;

    struct InteractivityType
    {
        InteractivityTypeSignature signature = InteractivityTypeSignature::Custom;
        /// The signature string as written in the asset (e.g. "float3").
        std::string signatureName;
    };

    struct InteractivityVariable
    {
        std::string name;
        /// Index into InteractivityGraph::types.
        int64_t type = -1;
        /// Initial value. Empty when not specified (the type's default applies).
        InteractivityValue value;
    };

    struct InteractivityEventValue
    {
        /// Index into InteractivityGraph::types.
        int64_t type = -1;
        /// Default value. Empty when not specified.
        InteractivityValue value;
    };

    struct InteractivityEvent
    {
        std::string name;
        /// Application-facing identifier of the custom event. Empty if not set.
        std::string id;
        std::map<std::string, InteractivityEventValue> values;
    };

    struct InteractivityDeclarationSocket
    {
        /// Index into InteractivityGraph::types.
        int64_t type = -1;
    };

    struct InteractivityDeclaration
    {
        /// Operation name, e.g. "event/onStart" or "math/add".
        std::string op;
        /// Extension that defines the operation. Empty for core operations.
        std::string extension;
        std::map<std::string, InteractivityDeclarationSocket> inputValueSockets;
        std::map<std::string, InteractivityDeclarationSocket> outputValueSockets;
    };

    /// An input value socket of a node: either an inline literal or a link to another node's output.
    struct InteractivityNodeValue
    {
        /// Source node index when the socket is linked; -1 for an inline literal.
        int64_t node = -1;
        /// Output value socket of the source node. Empty if not set.
        std::string socket;
        /// Index into InteractivityGraph::types for an inline literal; -1 if not set.
        int64_t type = -1;
        /// Inline literal. Empty when the socket is linked.
        InteractivityValue value;

        bool isLinked() const { return node >= 0; }
    };

    /// An output flow socket of a node, pointing at an input flow socket of another node.
    struct InteractivityFlow
    {
        int64_t node = -1;
        /// Target input flow socket. Defaults to "in".
        std::string socket = "in";
    };

    struct InteractivityNode
    {
        /// Index into InteractivityGraph::declarations.
        int64_t declaration = -1;
        std::map<std::string, InteractivityNodeValue> values;
        std::map<std::string, InteractivityFlow> flows;
        std::map<std::string, InteractivityValue> configuration;
    };

    struct InteractivityGraph
    {
        /// False when the graph breaks a "reject the graph" rule; it must not be executed.
        bool valid = true;
        /// Why the graph was rejected. Empty when valid.
        std::string error;

        std::string name;
        std::vector<InteractivityType> types;
        std::vector<InteractivityVariable> variables;
        std::vector<InteractivityEvent> events;
        std::vector<InteractivityDeclaration> declarations;
        std::vector<InteractivityNode> nodes;
    };

    struct KHRInteractivity
    {
        /**
         * False when the extension object is invalid (a structural rule is broken,
         * or the default graph is missing or invalid). The asset must then be
         * treated as having no interactivity; graphs is empty.
         */
        bool valid = true;
        /// Why the extension was rejected. Empty when valid.
        std::string error;

        /// Index of the graph to run by default.
        int64_t graph = 0;
        std::vector<InteractivityGraph> graphs;
    };

}
