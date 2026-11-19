// KHR_interactivity operation catalog and per-node socket/type checking.
//
// Socket tables come from the "Operations" chapter of the specification
// (https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_interactivity/Specification.adoc).

#include "khr_interactivity_ops.hpp"

#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace systems::leal::gltf;

namespace
{
    using Sig = InteractivityTypeSignature;

    // ---- types --------------------------------------------------------------

    // The type of a value socket. Custom types are told apart by their index in the graph's types array.
    struct TypeRef
    {
        Sig sig = Sig::Custom;
        int64_t custom = -1;
        bool known = false;

        static TypeRef of(Sig s) { return TypeRef{s, -1, true}; }
        static TypeRef unknown() { return TypeRef{}; }

        bool operator==(const TypeRef &o) const
        {
            return sig == o.sig && (sig != Sig::Custom || custom == o.custom);
        }
        bool operator!=(const TypeRef &o) const { return !(*this == o); }
    };

    const char *sigName(Sig s)
    {
        switch (s)
        {
        case Sig::Bool: return "bool";
        case Sig::Int: return "int";
        case Sig::Float: return "float";
        case Sig::Float2: return "float2";
        case Sig::Float3: return "float3";
        case Sig::Float4: return "float4";
        case Sig::Float2x2: return "float2x2";
        case Sig::Float3x3: return "float3x3";
        case Sig::Float4x4: return "float4x4";
        case Sig::Ref: return "ref";
        case Sig::Custom: return "custom";
        }
        return "?";
    }

    TypeRef typeAt(const InteractivityGraph &graph, int64_t index)
    {
        if (index < 0 || static_cast<uint64_t>(index) >= graph.types.size())
            return TypeRef::unknown();
        TypeRef t = TypeRef::of(graph.types[static_cast<size_t>(index)].signature);
        if (t.sig == Sig::Custom)
            t.custom = index;
        return t;
    }

    // ---- catalog --------------------------------------------------------------

    // A socket type in the table: a concrete signature, or a family placeholder bound once per overload.
    //   F = floatN or floatNxN, V = floatN, M = floatNxN, T = any type (including ref and custom).
    struct SocketType
    {
        char family = 0; // 0 when concrete
        Sig sig = Sig::Float;

        bool accepts(const TypeRef &t) const
        {
            switch (family)
            {
            case 0: return t == TypeRef::of(sig);
            case 'T': return true;
            case 'V': return t.sig == Sig::Float || t.sig == Sig::Float2 || t.sig == Sig::Float3 || t.sig == Sig::Float4;
            case 'M': return t.sig == Sig::Float2x2 || t.sig == Sig::Float3x3 || t.sig == Sig::Float4x4;
            case 'F': return SocketType{'V'}.accepts(t) || SocketType{'M'}.accepts(t);
            }
            return false;
        }
    };

    struct Socket
    {
        std::string name;
        SocketType type;
    };

    struct Overload
    {
        std::vector<Socket> inputs;
        std::vector<Socket> outputs;
    };

    // Value sockets of every operation with fixed sockets. Format per entry:
    //   "op: inputs > outputs | inputs > outputs ..." (one group per overload), sockets as "name:type".
    // Operations whose sockets depend on configuration are handled in code (see checkSpecial).
    const char *const kCatalog[] = {
        // constants
        "math/E: > value:float",
        "math/Pi: > value:float",
        "math/Tau: > value:float",
        "math/Inf: > value:float",
        "math/NaN: > value:float",
        "math/random: > value:float",
        // arithmetic
        "math/abs: a:F > value:F | a:int > value:int",
        "math/sign: a:F > value:F | a:int > value:int",
        "math/trunc: a:F > value:F",
        "math/floor: a:F > value:F",
        "math/ceil: a:F > value:F",
        "math/round: a:F > value:F",
        "math/fract: a:F > value:F",
        "math/neg: a:F > value:F | a:int > value:int",
        "math/add: a:F b:F > value:F | a:int b:int > value:int",
        "math/sub: a:F b:F > value:F | a:int b:int > value:int",
        "math/mul: a:F b:F > value:F | a:int b:int > value:int",
        "math/div: a:F b:F > value:F | a:int b:int > value:int",
        "math/rem: a:F b:F > value:F | a:int b:int > value:int",
        "math/min: a:F b:F > value:F | a:int b:int > value:int",
        "math/max: a:F b:F > value:F | a:int b:int > value:int",
        "math/clamp: a:F b:F c:F > value:F | a:int b:int c:int > value:int",
        "math/saturate: a:F > value:F",
        "math/mix: a:F b:F c:F > value:F",
        "math/smoothStep: a:V b:V c:V > value:V",
        // comparison
        "math/eq: a:F b:F > value:bool | a:int b:int > value:bool | a:bool b:bool > value:bool",
        "math/lt: a:float b:float > value:bool | a:int b:int > value:bool",
        "math/le: a:float b:float > value:bool | a:int b:int > value:bool",
        "math/gt: a:float b:float > value:bool | a:int b:int > value:bool",
        "math/ge: a:float b:float > value:bool | a:int b:int > value:bool",
        // special
        "math/isNaN: a:float > value:bool",
        "math/isInf: a:float > value:bool",
        // angle, trigonometry, hyperbolic, exponential
        "math/rad: a:V > value:V",
        "math/deg: a:V > value:V",
        "math/sin: a:V > value:V",
        "math/cos: a:V > value:V",
        "math/tan: a:V > value:V",
        "math/asin: a:V > value:V",
        "math/acos: a:V > value:V",
        "math/atan: a:V > value:V",
        "math/atan2: a:V b:V > value:V",
        "math/sinh: a:V > value:V",
        "math/cosh: a:V > value:V",
        "math/tanh: a:V > value:V",
        "math/asinh: a:V > value:V",
        "math/acosh: a:V > value:V",
        "math/atanh: a:V > value:V",
        "math/exp: a:V > value:V",
        "math/log: a:V > value:V",
        "math/log2: a:V > value:V",
        "math/log10: a:V > value:V",
        "math/sqrt: a:V > value:V",
        "math/cbrt: a:V > value:V",
        "math/pow: a:V b:V > value:V",
        // vector
        "math/length: a:V > value:float",
        "math/normalize: a:V > value:V isValid:bool",
        "math/dot: a:V b:V > value:float",
        "math/cross: a:float3 b:float3 > value:float3",
        "math/rotate2D: a:float2 angle:float > value:float2",
        "math/rotate3D: a:float3 rotation:float4 > value:float3",
        "math/transform: a:float2 b:float2x2 > value:float2 | a:float3 b:float3x3 > value:float3 | a:float4 b:float4x4 > value:float4",
        "math/slerp: a:float2 b:float2 c:float > value:float2 | a:float3 b:float3 c:float > value:float3",
        // matrix
        "math/transpose: a:M > value:M",
        "math/determinant: a:M > value:float",
        "math/inverse: a:M > value:M isValid:bool",
        "math/matMul: a:M b:M > value:M",
        "math/matCompose: translation:float3 rotation:float4 scale:float3 > value:float4x4",
        "math/matDecompose: a:float4x4 > translation:float3 rotation:float4 scale:float3",
        // quaternion
        "math/quatConjugate: a:float4 > value:float4",
        "math/quatMul: a:float4 b:float4 > value:float4",
        "math/quatAngleBetween: a:float4 b:float4 > value:float",
        "math/quatFromAxisAngle: axis:float3 angle:float > value:float4",
        "math/quatToAxisAngle: a:float4 > axis:float3 angle:float",
        "math/quatFromDirections: a:float3 b:float3 > value:float4",
        "math/quatFromUpForward: up:float3 forward:float3 > value:float4",
        "math/quatFromAngles: x:float y:float z:float > value:float4",
        "math/quatSlerp: a:float4 b:float4 c:float > value:float4",
        // swizzle
        "math/combine2: a:float b:float > value:float2",
        "math/combine3: a:float b:float c:float > value:float3",
        "math/combine4: a:float b:float c:float d:float > value:float4",
        "math/combine2x2: a:float b:float c:float d:float > value:float2x2",
        "math/combine3x3: a:float b:float c:float d:float e:float f:float g:float h:float i:float > value:float3x3",
        "math/combine4x4: a:float b:float c:float d:float e:float f:float g:float h:float i:float j:float k:float l:float"
        " m:float n:float o:float p:float > value:float4x4",
        "math/extract2: a:float2 > 0:float 1:float",
        "math/extract3: a:float3 > 0:float 1:float 2:float",
        "math/extract4: a:float4 > 0:float 1:float 2:float 3:float",
        "math/extract2x2: a:float2x2 > 0:float 1:float 2:float 3:float",
        "math/extract3x3: a:float3x3 > 0:float 1:float 2:float 3:float 4:float 5:float 6:float 7:float 8:float",
        "math/extract4x4: a:float4x4 > 0:float 1:float 2:float 3:float 4:float 5:float 6:float 7:float 8:float"
        " 9:float 10:float 11:float 12:float 13:float 14:float 15:float",
        // integer bitwise and boolean
        "math/not: a:int > value:int | a:bool > value:bool",
        "math/and: a:int b:int > value:int | a:bool b:bool > value:bool",
        "math/or: a:int b:int > value:int | a:bool b:bool > value:bool",
        "math/xor: a:int b:int > value:int | a:bool b:bool > value:bool",
        "math/asr: a:int b:int > value:int",
        "math/lsl: a:int b:int > value:int",
        "math/clz: a:int > value:int",
        "math/ctz: a:int > value:int",
        "math/popcnt: a:int > value:int",
        // color
        "math/rgbToOkLCh: r:float g:float b:float > l:float c:float h:float",
        "math/rgbFromOkLCh: l:float c:float h:float > r:float g:float b:float",
        // reference and type conversion
        "ref/eq: a:ref b:ref > value:bool",
        "type/boolToInt: a:bool > value:int",
        "type/boolToFloat: a:bool > value:float",
        "type/intToBool: a:int > value:bool",
        "type/intToFloat: a:int > value:float",
        "type/floatToBool: a:float > value:bool",
        "type/floatToInt: a:float > value:int",
        // control flow
        "flow/sequence: >",
        "flow/branch: condition:bool >",
        "flow/switch: selection:int >",
        "flow/while: condition:bool >",
        "flow/for: startIndex:int endIndex:int > index:int",
        "flow/doN: n:int > currentCount:int",
        "flow/multiGate: > lastIndex:int",
        "flow/waitAll: > remainingInputs:int",
        "flow/throttle: duration:float > lastRemainingTime:float",
        "flow/setDelay: duration:float > lastDelay:ref",
        "flow/cancelDelay: delay:ref >",
        // animation control
        "animation/start: animation:ref startTime:float endTime:float speed:float >",
        "animation/stop: animation:ref >",
        "animation/stopAt: animation:ref stopTime:float >",
        // events
        "event/onStart: > event:ref",
        "event/onTick: > timeSinceStart:float timeSinceLastTick:float event:ref",
        "event/stopPropagation: stopImmediate:bool event:ref >",
    };

    // Operations whose sockets come from their configuration (see checkSpecial).
    const char *const kSpecialOps[] = {
        "math/select", "math/switch",
        "variable/get", "variable/set", "variable/interpolate",
        "pointer/get", "pointer/set", "pointer/interpolate",
        "event/receive", "event/send", "debug/log",
    };

    SocketType parseSocketType(const std::string &s)
    {
        if (s.size() == 1)
            return SocketType{s[0]};
        static const std::pair<const char *, Sig> kNames[] = {
            {"bool", Sig::Bool}, {"int", Sig::Int}, {"float", Sig::Float}, {"float2", Sig::Float2},
            {"float3", Sig::Float3}, {"float4", Sig::Float4}, {"float2x2", Sig::Float2x2},
            {"float3x3", Sig::Float3x3}, {"float4x4", Sig::Float4x4}, {"ref", Sig::Ref},
        };
        for (const auto &n : kNames)
            if (s == n.first)
                return SocketType{0, n.second};
        throw std::logic_error("KHR_interactivity catalog: unknown socket type " + s);
    }

    std::vector<Socket> parseSockets(const std::string &text)
    {
        std::vector<Socket> sockets;
        std::istringstream in(text);
        std::string token;
        while (in >> token)
        {
            const auto colon = token.find(':');
            sockets.push_back(Socket{token.substr(0, colon), parseSocketType(token.substr(colon + 1))});
        }
        return sockets;
    }

    const std::unordered_map<std::string, std::vector<Overload>> &catalog()
    {
        static const auto table = [] {
            std::unordered_map<std::string, std::vector<Overload>> t;
            for (const char *entry : kCatalog)
            {
                const std::string line(entry);
                const auto colon = line.find(": ");
                auto &overloads = t[line.substr(0, colon)];
                std::string rest = line.substr(colon + 2);
                size_t start = 0;
                while (start <= rest.size())
                {
                    const auto bar = rest.find('|', start);
                    const std::string group = rest.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
                    const auto gt = group.find('>');
                    overloads.push_back(Overload{parseSockets(group.substr(0, gt)), parseSockets(group.substr(gt + 1))});
                    if (bar == std::string::npos)
                        break;
                    start = bar + 1;
                }
            }
            return t;
        }();
        return table;
    }

    bool isSpecialOp(const std::string &op)
    {
        for (const char *s : kSpecialOps)
            if (op == s)
                return true;
        return false;
    }

    // ---- configuration helpers ------------------------------------------------

    // An "int" configuration value: one number exactly representable as a 32-bit signed integer.
    bool literalInt32(const InteractivityLiteral &lit, int64_t &out)
    {
        double d = 0;
        if (lit.kind == InteractivityLiteral::Kind::Integer)
            d = static_cast<double>(lit.intValue);
        else if (lit.kind == InteractivityLiteral::Kind::Number)
            d = static_cast<double>(lit.numberValue);
        else
            return false;
        if (!(d >= -2147483648.0 && d <= 2147483647.0) || std::floor(d) != d)
            return false;
        out = static_cast<int64_t>(d);
        return true;
    }

    const InteractivityValue *config(const InteractivityNode &node, const char *key)
    {
        const auto it = node.configuration.find(key);
        return it == node.configuration.end() ? nullptr : &it->second;
    }

    bool configInt(const InteractivityNode &node, const char *key, int64_t &out)
    {
        const auto *v = config(node, key);
        return v && v->size() == 1 && literalInt32((*v)[0], out);
    }

    bool configString(const InteractivityNode &node, const char *key, std::string &out)
    {
        const auto *v = config(node, key);
        if (!v || v->size() != 1 || (*v)[0].kind != InteractivityLiteral::Kind::String)
            return false;
        out = (*v)[0].stringValue;
        return true;
    }

    // Unescapes RFC 6901 "~1" -> "/" and "~0" -> "~".
    std::string unescapePointerToken(const std::string &s)
    {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '~' && i + 1 < s.size())
            {
                out += s[i + 1] == '1' ? '/' : '~';
                ++i;
            }
            else
                out += s[i];
        }
        return out;
    }

    // An odd-length run of `c` anywhere in a literal path segment.
    bool hasOddRun(const std::string &segment, char c)
    {
        for (size_t i = 0; i < segment.size();)
        {
            if (segment[i] != c)
            {
                ++i;
                continue;
            }
            size_t j = i;
            while (j < segment.size() && segment[j] == c)
                ++j;
            if ((j - i) % 2 == 1)
                return true;
            i = j;
        }
        return false;
    }

    struct TemplateParam
    {
        std::string id;
        bool isRef = false;
    };

    // "JSON Pointer Template Parsing". Returns false for a syntax error.
    bool parsePointerTemplate(const std::string &pointer, std::vector<TemplateParam> &params)
    {
        // RFC 6901 syntax: empty or '/'-prefixed, '~' only as "~0" / "~1".
        if (!pointer.empty() && pointer[0] != '/')
            return false;
        for (size_t i = 0; i < pointer.size(); ++i)
            if (pointer[i] == '~' && (i + 1 >= pointer.size() || (pointer[i + 1] != '0' && pointer[i + 1] != '1')))
                return false;

        std::set<std::string> ids;
        size_t start = 0;
        while (true)
        {
            const auto slash = pointer.find('/', start);
            const std::string segment = pointer.substr(start, slash == std::string::npos ? std::string::npos : slash - start);

            if (segment == "[" || segment == "{")
                return false;
            const bool intParam = segment.size() >= 1 && segment[0] == '[' && (segment.size() < 2 || segment[1] != '[');
            const bool refParam = segment.size() >= 1 && segment[0] == '{' && (segment.size() < 2 || segment[1] != '{');
            if (intParam || refParam)
            {
                const char close = intParam ? ']' : '}';
                if (segment.size() < 3 || segment.back() != close)
                    return false;
                const std::string inner = segment.substr(1, segment.size() - 2);
                if (inner.find_first_of("[]{}") != std::string::npos)
                    return false;
                const std::string id = unescapePointerToken(inner);
                if (!ids.insert(id).second)
                    return false;
                params.push_back(TemplateParam{id, refParam});
            }
            else if (hasOddRun(segment, '[') || hasOddRun(segment, ']') || hasOddRun(segment, '{') || hasOddRun(segment, '}'))
            {
                return false;
            }

            if (slash == std::string::npos)
                break;
            start = slash + 1;
        }
        return true;
    }

    // "debug/log" message template parsing. Returns false when the message is syntactically invalid.
    bool parseMessageTemplate(const std::string &message, std::vector<std::string> &params)
    {
        int state = 0;
        size_t paramStart = 0;
        for (size_t i = 0; i < message.size(); ++i)
        {
            const char c = message[i];
            if (c == '{')
            {
                if (state == 0) state = 1;
                else if (state == 1) state = 0;
                else return false;
            }
            else if (c == '}')
            {
                if (state == 0) state = 3;
                else if (state == 3) state = 0;
                else if (state == 2)
                {
                    params.push_back(message.substr(paramStart + 1, i - paramStart - 1));
                    state = 0;
                }
                else return false;
            }
            else
            {
                if (state == 1)
                {
                    paramStart = i - 1;
                    state = 2;
                }
                else if (state == 3)
                    return false;
            }
        }
        return state == 0;
    }

    // ---- checker ------------------------------------------------------------------

    class OperationChecker
    {
    public:
        OperationChecker(InteractivityGraph &graph, const std::string &context)
            : graph_(graph), context_(context), outputs_(graph.nodes.size()), resolved_(graph.nodes.size(), false)
        {
        }

        void run()
        {
            checkDeclarations();
            for (size_t n = 0; n < graph_.nodes.size(); ++n)
                checkNode(n);
        }

    private:
        InteractivityGraph &graph_;
        const std::string &context_;
        // Output value socket types of each node, filled once the node's operation is resolved.
        std::vector<std::map<std::string, TypeRef>> outputs_;
        std::vector<bool> resolved_;

        void reject(const std::string &ctx, const std::string &message)
        {
            if (graph_.valid)
            {
                graph_.valid = false;
                graph_.error = ctx + " " + message;
            }
        }

        const InteractivityDeclaration *declarationOf(const InteractivityNode &node) const
        {
            if (node.declaration < 0 || static_cast<uint64_t>(node.declaration) >= graph_.declarations.size())
                return nullptr;
            return &graph_.declarations[static_cast<size_t>(node.declaration)];
        }

        // Every declaration without an extension must name an operation of this Specification.
        void checkDeclarations()
        {
            for (size_t d = 0; d < graph_.declarations.size(); ++d)
            {
                const auto &decl = graph_.declarations[d];
                if (decl.extension.empty() && !isSpecialOp(decl.op) && catalog().find(decl.op) == catalog().end())
                    reject(context_ + ".declarations[" + std::to_string(d) + "]", "uses unknown operation '" + decl.op + "'");
            }
        }

        // Effective type of an input value: its explicit type, or the type of the linked output.
        // Also checks that a linked output exists and matches an explicit type.
        TypeRef inputType(const InteractivityNodeValue &value, const std::string &ctx)
        {
            if (!value.isLinked())
                return typeAt(graph_, value.type);
            if (static_cast<uint64_t>(value.node) >= graph_.nodes.size() || !resolved_[static_cast<size_t>(value.node)])
                return TypeRef::unknown(); // forward link (already rejected) or unresolved source
            const std::string socket = value.socket.empty() ? "value" : value.socket;
            const auto &outs = outputs_[static_cast<size_t>(value.node)];
            const auto it = outs.find(socket);
            if (it == outs.end())
            {
                reject(ctx, "reads output value '" + socket + "' that node " + std::to_string(value.node) + " does not have");
                return TypeRef::unknown();
            }
            if (value.type >= 0)
            {
                const TypeRef declared = typeAt(graph_, value.type);
                if (declared.known && it->second.known && declared != it->second)
                    reject(ctx, "declares type " + std::string(sigName(declared.sig)) + " but output '" + socket + "' is " + sigName(it->second.sig));
            }
            return it->second;
        }

        void checkNode(size_t n)
        {
            const auto &node = graph_.nodes[n];
            const std::string ctx = context_ + ".nodes[" + std::to_string(n) + "]";

            std::map<std::string, TypeRef> inputs;
            for (const auto &value : node.values)
                inputs[value.first] = inputType(value.second, ctx + ".values['" + value.first + "']");

            const InteractivityDeclaration *decl = declarationOf(node);
            if (!decl)
                return;

            std::map<std::string, TypeRef> outs;
            bool ok = true;
            if (!decl->extension.empty())
                ok = checkDeclared(*decl, inputs, ctx, outs);
            else if (isSpecialOp(decl->op))
                ok = checkSpecial(*decl, node, inputs, ctx, outs);
            else
            {
                const auto it = catalog().find(decl->op);
                if (it == catalog().end())
                    return; // unknown operation, already rejected
                ok = matchOverloads(decl->op, it->second, inputs, ctx, outs);
            }
            if (ok)
            {
                outputs_[n] = std::move(outs);
                resolved_[n] = true;
            }
        }

        // Inputs required by `required` must be present with the given types.
        bool requireInputs(const std::map<std::string, TypeRef> &inputs, const std::vector<std::pair<std::string, TypeRef>> &required,
                           const std::string &ctx, const std::string &op)
        {
            for (const auto &r : required)
            {
                const auto it = inputs.find(r.first);
                if (it == inputs.end())
                {
                    reject(ctx, "'" + op + "' is missing input value '" + r.first + "'");
                    return false;
                }
                if (r.second.known && it->second.known && it->second != r.second)
                {
                    reject(ctx, "'" + op + "' input value '" + r.first + "' must be " + sigName(r.second.sig) + ", not " + sigName(it->second.sig));
                    return false;
                }
            }
            return true;
        }

        // Operation from an additional extension: the declaration defines its value sockets.
        bool checkDeclared(const InteractivityDeclaration &decl, const std::map<std::string, TypeRef> &inputs,
                           const std::string &ctx, std::map<std::string, TypeRef> &outs)
        {
            std::vector<std::pair<std::string, TypeRef>> required;
            for (const auto &s : decl.inputValueSockets)
                required.emplace_back(s.first, typeAt(graph_, s.second.type));
            if (!requireInputs(inputs, required, ctx, decl.op))
                return false;
            for (const auto &s : decl.outputValueSockets)
                outs[s.first] = typeAt(graph_, s.second.type);
            return true;
        }

        bool matchOverloads(const std::string &op, const std::vector<Overload> &overloads, const std::map<std::string, TypeRef> &inputs,
                            const std::string &ctx, std::map<std::string, TypeRef> &outs)
        {
            std::string missing;
            for (const auto &overload : overloads)
            {
                bool match = true;
                TypeRef bound = TypeRef::unknown();
                for (const auto &socket : overload.inputs)
                {
                    const auto it = inputs.find(socket.name);
                    if (it == inputs.end())
                    {
                        if (missing.empty())
                            missing = socket.name;
                        match = false;
                        break;
                    }
                    const TypeRef &t = it->second;
                    if (!t.known)
                        continue; // unresolved source; already rejected elsewhere
                    if (!socket.type.accepts(t))
                    {
                        match = false;
                        break;
                    }
                    if (socket.type.family)
                    {
                        if (!bound.known)
                            bound = t;
                        else if (bound != t)
                        {
                            match = false;
                            break;
                        }
                    }
                }
                if (!match)
                    continue;
                for (const auto &socket : overload.outputs)
                    outs[socket.name] = socket.type.family ? bound : TypeRef::of(socket.type.sig);
                return true;
            }
            if (!missing.empty())
                reject(ctx, "'" + op + "' is missing input value '" + missing + "'");
            else
                reject(ctx, "'" + op + "' does not support the given input value types");
            return false;
        }

        // ---- configurable operations ----------------------------------------------------

        bool variableIndex(const InteractivityNode &node, const char *key, const std::string &ctx, const std::string &op, int64_t &index)
        {
            if (!configInt(node, key, index) || index < 0 || static_cast<uint64_t>(index) >= graph_.variables.size())
            {
                reject(ctx, "'" + op + "' needs a valid '" + key + "' variable index configuration");
                return false;
            }
            return true;
        }

        TypeRef variableType(int64_t index) const
        {
            return typeAt(graph_, graph_.variables[static_cast<size_t>(index)].type);
        }

        bool eventIndex(const InteractivityNode &node, const std::string &ctx, const std::string &op, int64_t &index)
        {
            if (!configInt(node, "event", index) || index < 0 || static_cast<uint64_t>(index) >= graph_.events.size())
            {
                reject(ctx, "'" + op + "' needs a valid 'event' index configuration");
                return false;
            }
            return true;
        }

        // pointer/* configuration: template parameters become int / ref inputs; `type` gives T.
        bool pointerConfig(const InteractivityNode &node, const std::string &ctx, const std::string &op,
                           std::vector<std::pair<std::string, TypeRef>> &required, TypeRef &type,
                           std::initializer_list<const char *> reservedIds)
        {
            std::string pointer;
            std::vector<TemplateParam> params;
            if (!configString(node, "pointer", pointer) || !parsePointerTemplate(pointer, params))
            {
                reject(ctx, "'" + op + "' needs a valid 'pointer' template configuration");
                return false;
            }
            for (const auto &p : params)
            {
                for (const char *reserved : reservedIds)
                {
                    if (p.id == reserved)
                    {
                        reject(ctx, "'" + op + "' pointer template must not use the parameter '" + p.id + "'");
                        return false;
                    }
                }
                required.emplace_back(p.id, TypeRef::of(p.isRef ? Sig::Ref : Sig::Int));
            }
            int64_t typeIndex = -1;
            if (!configInt(node, "type", typeIndex) || typeIndex < 0 || static_cast<uint64_t>(typeIndex) >= graph_.types.size())
            {
                reject(ctx, "'" + op + "' needs a valid 'type' index configuration");
                return false;
            }
            type = typeAt(graph_, typeIndex);
            return true;
        }

        static bool interpolatable(const TypeRef &t)
        {
            return t.sig != Sig::Bool && t.sig != Sig::Int && t.sig != Sig::Ref;
        }

        bool checkSpecial(const InteractivityDeclaration &decl, const InteractivityNode &node, const std::map<std::string, TypeRef> &inputs,
                          const std::string &ctx, std::map<std::string, TypeRef> &outs)
        {
            const std::string &op = decl.op;
            std::vector<std::pair<std::string, TypeRef>> required;
            const TypeRef floatT = TypeRef::of(Sig::Float), float2T = TypeRef::of(Sig::Float2);

            if (op == "math/select")
            {
                required = {{"condition", TypeRef::of(Sig::Bool)}, {"a", TypeRef::unknown()}, {"b", TypeRef::unknown()}};
                if (!requireInputs(inputs, required, ctx, op))
                    return false;
                const TypeRef a = inputs.at("a"), b = inputs.at("b");
                if (a.known && b.known && a != b)
                {
                    reject(ctx, "'math/select' inputs 'a' and 'b' must have the same type");
                    return false;
                }
                outs["value"] = a.known ? a : b;
                return true;
            }
            if (op == "math/switch")
            {
                // Valid int32 cases add sockets; otherwise the default configuration (no cases) applies.
                std::vector<std::string> cases;
                if (const auto *v = config(node, "cases"))
                {
                    std::set<std::string> seen;
                    for (const auto &lit : *v)
                    {
                        int64_t c = 0;
                        if (!literalInt32(lit, c))
                        {
                            cases.clear();
                            break;
                        }
                        if (seen.insert(std::to_string(c)).second)
                            cases.push_back(std::to_string(c));
                    }
                }
                required = {{"selection", TypeRef::of(Sig::Int)}, {"default", TypeRef::unknown()}};
                if (!requireInputs(inputs, required, ctx, op))
                    return false;
                const TypeRef t = inputs.at("default");
                required.clear();
                for (const auto &c : cases)
                    required.emplace_back(c, t);
                if (!requireInputs(inputs, required, ctx, op))
                    return false;
                outs["value"] = t;
                return true;
            }
            if (op == "variable/get")
            {
                int64_t index = -1;
                if (!variableIndex(node, "variable", ctx, op, index))
                    return false;
                outs["value"] = variableType(index);
                return true;
            }
            if (op == "variable/set")
            {
                const auto *v = config(node, "variables");
                if (!v)
                {
                    reject(ctx, "'variable/set' needs a 'variables' configuration");
                    return false;
                }
                for (const auto &lit : *v)
                {
                    int64_t index = -1;
                    if (!literalInt32(lit, index) || index < 0 || static_cast<uint64_t>(index) >= graph_.variables.size())
                    {
                        reject(ctx, "'variable/set' configuration has an invalid variable index");
                        return false;
                    }
                    required.emplace_back(std::to_string(index), variableType(index));
                }
                return requireInputs(inputs, required, ctx, op);
            }
            if (op == "variable/interpolate")
            {
                int64_t index = -1;
                if (!variableIndex(node, "variable", ctx, op, index))
                    return false;
                const TypeRef t = variableType(index);
                if (!interpolatable(t))
                {
                    reject(ctx, "'variable/interpolate' cannot interpolate a variable of type " + std::string(sigName(t.sig)));
                    return false;
                }
                const auto *slerp = config(node, "useSlerp");
                if (!slerp || slerp->size() != 1 || (*slerp)[0].kind != InteractivityLiteral::Kind::Bool)
                {
                    reject(ctx, "'variable/interpolate' needs a boolean 'useSlerp' configuration");
                    return false;
                }
                if ((*slerp)[0].boolValue && t.sig != Sig::Float4)
                {
                    reject(ctx, "'variable/interpolate' useSlerp requires a float4 variable");
                    return false;
                }
                required = {{"value", t}, {"duration", floatT}, {"p1", float2T}, {"p2", float2T}};
                return requireInputs(inputs, required, ctx, op);
            }
            if (op == "pointer/get" || op == "pointer/set" || op == "pointer/interpolate")
            {
                TypeRef t;
                const bool isGet = op == "pointer/get", isSet = op == "pointer/set";
                const bool configured = isGet ? pointerConfig(node, ctx, op, required, t, {})
                                      : isSet ? pointerConfig(node, ctx, op, required, t, {"value"})
                                              : pointerConfig(node, ctx, op, required, t, {"value", "duration", "p1", "p2"});
                if (!configured)
                    return false;
                if (!isGet && !isSet && !interpolatable(t))
                {
                    reject(ctx, "'pointer/interpolate' cannot interpolate a property of type " + std::string(sigName(t.sig)));
                    return false;
                }
                if (!isGet)
                    required.emplace_back("value", t);
                if (!isGet && !isSet)
                {
                    required.emplace_back("duration", floatT);
                    required.emplace_back("p1", float2T);
                    required.emplace_back("p2", float2T);
                }
                if (!requireInputs(inputs, required, ctx, op))
                    return false;
                if (isGet)
                {
                    outs["value"] = t;
                    outs["isValid"] = TypeRef::of(Sig::Bool);
                }
                return true;
            }
            if (op == "event/receive" || op == "event/send")
            {
                int64_t index = -1;
                if (!eventIndex(node, ctx, op, index))
                    return false;
                const auto &event = graph_.events[static_cast<size_t>(index)];
                if (op == "event/receive")
                {
                    for (const auto &v : event.values)
                        outs[v.first] = typeAt(graph_, v.second.type);
                    outs["event"] = TypeRef::of(Sig::Ref);
                    return true;
                }
                for (const auto &v : event.values)
                    required.emplace_back(v.first, typeAt(graph_, v.second.type));
                return requireInputs(inputs, required, ctx, op);
            }
            if (op == "debug/log")
            {
                // An invalid or missing message falls back to the default configuration (no parameters).
                std::string message;
                std::vector<std::string> params;
                if (configString(node, "message", message) && parseMessageTemplate(message, params))
                {
                    for (const auto &p : params)
                        required.emplace_back(p, TypeRef::unknown());
                }
                return requireInputs(inputs, required, ctx, op);
            }
            return true;
        }
    };
}

void checkInteractivityOperations(InteractivityGraph &graph, const std::string &context)
{
    OperationChecker(graph, context).run();
}
