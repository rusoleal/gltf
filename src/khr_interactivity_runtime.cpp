// KHR_interactivity runtime: executes a validated behavior graph.
//
// Execution model (specification, "Interactivity Nodes"): flow nodes run when
// one of their input flows is activated, and activate output flows
// synchronously (depth first). Nodes without flow sockets ("pure" nodes) are
// computed when one of their outputs is read; their outputs are kept until the
// next flow node runs (only observable with math/random). Flow nodes' output
// values are their internal state (flow/for's index, event outputs, ...).
//
// Operations the runtime doesn't implement are the specification's no-ops.

#include <gltf/extensions/khr_interactivity_runtime.hpp>
#include <gltf/gltf.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace systems::leal::gltf
{

using Sig = InteractivityTypeSignature;
using Value = InteractivityRuntimeValue;
using Ref = InteractivityReference;

namespace
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kPi = 3.14159265358979323846;
    // Activations a single event may run (endless loops end here).
    constexpr uint64_t kStepBudget = 200000;
    // Nested flow activations (loops in loops...): chains of flow nodes don't
    // nest (tailFire()) and values are computed without recursion.
    constexpr int kMaxDepth = 256;
    // Delays, animations and interpolations each may have this many active
    // (/extensions/KHR_interactivity/limits/...), and last at most kMaxDuration s.
    constexpr size_t kMaxActive = 1024;
    constexpr double kMaxDuration = 1.0e9;

    bool isFloatType(Sig s)
    {
        return s == Sig::Float || s == Sig::Float2 || s == Sig::Float3 || s == Sig::Float4 || s == Sig::Float2x2 ||
               s == Sig::Float3x3 || s == Sig::Float4x4;
    }
    bool isMatrix(Sig s) { return s == Sig::Float2x2 || s == Sig::Float3x3 || s == Sig::Float4x4; }
    int matrixSize(Sig s) { return s == Sig::Float2x2 ? 2 : s == Sig::Float3x3 ? 3 : 4; }
    Sig vectorType(int n) { return n == 1 ? Sig::Float : n == 2 ? Sig::Float2 : n == 3 ? Sig::Float3 : Sig::Float4; }

    int32_t wrap(int64_t v) { return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(v))); }

    // A literal number that is exactly a 32-bit signed integer.
    bool literalInt(const InteractivityLiteral &l, int32_t &out)
    {
        double v;
        if (l.kind == InteractivityLiteral::Kind::Integer) v = static_cast<double>(l.intValue);
        else if (l.kind == InteractivityLiteral::Kind::Number) v = static_cast<double>(l.numberValue);
        else return false;
        if (!(v >= -2147483648.0 && v <= 2147483647.0) || std::floor(v) != v) return false;
        out = static_cast<int32_t>(v);
        return true;
    }

    // "/nodes/3" -> {Object, "nodes", 3}; anything else is null.
    Ref parseObjectRef(const std::string &s)
    {
        Ref r;
        if (s.size() < 4 || s[0] != '/') return r;
        const auto slash = s.find('/', 1);
        if (slash == std::string::npos || slash + 1 >= s.size()) return r;
        const std::string index = s.substr(slash + 1);
        if (index.find_first_not_of("0123456789") != std::string::npos || index.size() > 10) return r;
        r.kind = Ref::Kind::Object;
        r.collection = s.substr(1, slash - 1);
        r.index = std::stoll(index);
        return r;
    }

    // Template parameters of a JSON Pointer template ("/nodes/[i]/x", "/materials/{m}/y").
    struct PointerTemplate
    {
        struct Param
        {
            std::string id;
            bool reference = false;
            size_t start = 0, length = 0;
        };
        std::string text;
        std::vector<Param> params;
        bool valid = false;
    };

    PointerTemplate parsePointerTemplate(const std::string &text)
    {
        PointerTemplate t;
        t.text = text;
        size_t pos = 0;
        while (pos <= text.size())
        {
            const size_t end = text.find('/', pos);
            const size_t stop = end == std::string::npos ? text.size() : end;
            const std::string seg = text.substr(pos, stop - pos);
            if (seg.size() >= 3 && ((seg.front() == '[' && seg[1] != '[' && seg.back() == ']') ||
                                    (seg.front() == '{' && seg[1] != '{' && seg.back() == '}')))
            {
                PointerTemplate::Param p;
                p.reference = seg.front() == '{';
                p.id = seg.substr(1, seg.size() - 2);
                // ~1 -> /, ~0 -> ~
                std::string id;
                for (size_t i = 0; i < p.id.size(); ++i)
                {
                    if (p.id[i] == '~' && i + 1 < p.id.size()) { id += p.id[i + 1] == '1' ? '/' : '~'; ++i; }
                    else id += p.id[i];
                }
                p.id = id;
                p.start = pos;
                p.length = seg.size();
                t.params.push_back(p);
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
        t.valid = true;
        return t;
    }

    std::string unescapeBrackets(std::string p)
    {
        for (const char *pair : {"[[", "]]", "{{", "}}"})
        {
            size_t at = 0;
            while ((at = p.find(pair, at)) != std::string::npos) { p.erase(at, 1); ++at; }
        }
        return p;
    }

    std::string formatValue(const Value &v)
    {
        std::ostringstream out;
        switch (v.type)
        {
        case Sig::Bool: out << (v.b ? "true" : "false"); break;
        case Sig::Int: out << v.i; break;
        case Sig::Ref:
            if (v.ref.kind == Ref::Kind::Object) out << "/" << v.ref.collection << "/" << v.ref.index;
            else if (v.ref.kind == Ref::Kind::Null) out << "null";
            else out << "<" << (v.ref.kind == Ref::Kind::Event ? "event " : "delay ") << v.ref.id << ">";
            break;
        default:
        {
            const int n = Value::componentCount(v.type);
            if (n == 1) { out << v.f[0]; break; }
            out << "[";
            for (int k = 0; k < n; ++k) out << (k ? ", " : "") << v.f[k];
            out << "]";
        }
        }
        return out.str();
    }

    // Operations with flow sockets: their outputs are internal state.
    bool isFlowOperation(const std::string &op)
    {
        return op.rfind("flow/", 0) == 0 || op.rfind("event/", 0) == 0 || op == "variable/set" ||
               op == "variable/interpolate" || op == "pointer/set" || op == "pointer/interpolate" ||
               op.rfind("animation/", 0) == 0 || op == "debug/log";
    }
} // namespace

// ---- values --------------------------------------------------------------------

int Value::componentCount(Sig type)
{
    switch (type)
    {
    case Sig::Float: return 1;
    case Sig::Float2: return 2;
    case Sig::Float3: return 3;
    case Sig::Float4: return 4;
    case Sig::Float2x2: return 4;
    case Sig::Float3x3: return 9;
    case Sig::Float4x4: return 16;
    default: return 0;
    }
}

Value Value::defaultOf(Sig type)
{
    Value v;
    v.type = type;
    v.f.fill(kNaN);
    return v;
}

Value Value::ofBool(bool b)
{
    Value v = defaultOf(Sig::Bool);
    v.b = b;
    return v;
}

Value Value::ofInt(int32_t i)
{
    Value v = defaultOf(Sig::Int);
    v.i = i;
    return v;
}

Value Value::ofFloat(double f)
{
    Value v = defaultOf(Sig::Float);
    v.f[0] = f;
    return v;
}

Value Value::ofFloats(Sig type, const double *values)
{
    Value v = defaultOf(type);
    for (int k = 0; k < componentCount(type); ++k) v.f[k] = values[k];
    return v;
}

Value Value::ofRef(const Ref &r)
{
    Value v = defaultOf(Sig::Ref);
    v.ref = r;
    return v;
}

double InteractivityHost::random()
{
    static std::mt19937_64 engine{std::random_device{}()};
    return std::uniform_real_distribution<double>(0.0, 1.0)(engine);
}

// ---- runtime -------------------------------------------------------------------

struct InteractivityRuntime::Impl
{
    const GLTF &asset;
    InteractivityHost &host;
    const InteractivityGraph *graph = nullptr;

    std::vector<int64_t> parent;           // glTF node -> parent (-1: root)
    std::vector<std::string> ops;          // interactivity node -> operation
    std::vector<bool> flowNode;            // interactivity node has flow sockets
    std::vector<bool> supported;           // interactivity node's operation is implemented
    std::vector<PointerTemplate> pointers; // pointer/* nodes' templates

    struct NodeState
    {
        std::map<std::string, Value> outputs; // flow nodes' output values
        std::vector<bool> used;               // multiGate outputs, waitAll inputs
        double timestamp = 0;                 // throttle
        std::vector<uint64_t> delays;         // setDelay activations
    };
    std::vector<NodeState> state;
    std::vector<Value> variables;
    std::unordered_map<int64_t, std::map<std::string, Value>> pureCache;

    struct Delay
    {
        uint64_t id;
        double at;
        int64_t node;
    };
    std::vector<Delay> delays;
    uint64_t nextId = 1;

    struct Received
    {
        int64_t event;
        std::map<std::string, Value> values;
    };
    std::vector<Received> queued; // event/send to the graph's own receivers: next tick

    // variable/interpolate and pointer/interpolate in progress.
    struct Interpolation
    {
        uint64_t id;
        int32_t variable = -1; // variable/interpolate
        std::string pointer;   // pointer/interpolate
        double start = 0, duration = 0;
        Value from, to;
        double p1[2] = {0, 0}, p2[2] = {1, 1};
        bool slerp = false;
        int64_t node = -1; // its `done` flow
    };
    std::vector<Interpolation> interpolations;

    // animation/start: the animations the graph plays (the specification's
    // animation state array).
    struct Playback
    {
        uint64_t id;
        int64_t animation;
        double startTime, endTime, stopTime, speed;
        double created;       // runtime time at activation
        int64_t endNode;      // animation/start's `done`
        int64_t stopNode = -1; // animation/stopAt's `done`
    };
    std::vector<Playback> playbacks;

    // Per glTF animation: the read-only Animation State properties.
    struct AnimationInfo
    {
        bool valid = false;
        double minTime = kNaN, maxTime = kNaN;
        double playhead = kNaN, virtualPlayhead = kNaN;
    };
    std::vector<AnimationInfo> animations;

    std::map<int32_t, int64_t> hovered; // controller -> hovered node

    double time = 0;         // seconds since start()
    bool started = false;
    bool ticked = false;
    double lastTick = 0;

    // The event being processed (event/stopPropagation).
    Ref currentEvent;
    bool stopTransitive = false;
    bool stopImmediate = false;

    uint64_t steps = 0;
    int depth = 0;
    bool aborted = false;
    int64_t nextNode = -1; // tailFire()'s target
    std::string nextSocket;

    std::vector<std::string> unsupported;
    std::vector<std::string> errors;

    Impl(const GLTF &a, InteractivityHost &h) : asset(a), host(h)
    {
        const auto &ix = asset.khrInteractivity;
        if (!ix || !ix->valid || ix->graphs.empty() || ix->graph < 0 ||
            static_cast<size_t>(ix->graph) >= ix->graphs.size() || !ix->graphs[ix->graph].valid)
            return;
        graph = &ix->graphs[static_cast<size_t>(ix->graph)];

        if (asset.nodes)
        {
            parent.assign(asset.nodes->size(), -1);
            for (size_t i = 0; i < asset.nodes->size(); ++i)
                for (auto c : (*asset.nodes)[i].children)
                    if (c < parent.size()) parent[c] = static_cast<int64_t>(i);
        }

        const size_t n = graph->nodes.size();
        ops.resize(n);
        flowNode.resize(n);
        supported.resize(n);
        pointers.resize(n);
        state.resize(n);
        std::set<std::string> missing;
        for (size_t i = 0; i < n; ++i)
        {
            const auto &node = graph->nodes[i];
            const auto &decl = graph->declarations[static_cast<size_t>(node.declaration)];
            ops[i] = decl.op;
            flowNode[i] = isFlowOperation(decl.op);
            supported[i] = implemented(decl.op);
            if (!supported[i]) missing.insert(decl.op);
            if (decl.op.rfind("pointer/", 0) == 0)
            {
                const auto it = node.configuration.find("pointer");
                if (it != node.configuration.end() && !it->second.empty() &&
                    it->second[0].kind == InteractivityLiteral::Kind::String)
                    pointers[i] = parsePointerTemplate(it->second[0].stringValue);
            }
        }
        unsupported.assign(missing.begin(), missing.end());

        for (const auto &v : graph->variables) variables.push_back(literal(v.type, v.value));
        initAnimations();
        for (size_t i = 0; i < n; ++i) resetState(static_cast<int64_t>(i));
    }

    // ---- operations implemented ------------------------------------------------

    static bool implemented(const std::string &op)
    {
        static const std::unordered_set<std::string> ops = {
            // math
            "math/E", "math/Pi", "math/Tau", "math/Inf", "math/NaN", "math/random", "math/abs", "math/sign",
            "math/trunc", "math/floor", "math/ceil", "math/round", "math/fract", "math/neg", "math/add", "math/sub",
            "math/mul", "math/div", "math/rem", "math/min", "math/max", "math/clamp", "math/saturate", "math/mix",
            "math/smoothStep", "math/eq", "math/lt", "math/le", "math/gt", "math/ge", "math/isNaN", "math/isInf",
            "math/select", "math/switch", "math/rad", "math/deg", "math/sin", "math/cos", "math/tan", "math/asin",
            "math/acos", "math/atan", "math/atan2", "math/sinh", "math/cosh", "math/tanh", "math/asinh",
            "math/acosh", "math/atanh", "math/exp", "math/log", "math/log2", "math/log10", "math/sqrt", "math/cbrt",
            "math/pow", "math/length", "math/normalize", "math/dot", "math/cross", "math/rotate2D",
            "math/transform", "math/transpose", "math/determinant", "math/matMul", "math/combine2",
            "math/combine3", "math/combine4", "math/combine2x2", "math/combine3x3", "math/combine4x4",
            "math/extract2", "math/extract3", "math/extract4", "math/extract2x2", "math/extract3x3",
            "math/extract4x4", "math/inverse", "math/matCompose", "math/matDecompose", "math/rotate3D",
            "math/slerp", "math/quatConjugate", "math/quatMul", "math/quatAngleBetween", "math/quatFromAxisAngle",
            "math/quatToAxisAngle", "math/quatFromDirections", "math/quatFromUpForward", "math/quatFromAngles",
            "math/quatSlerp", "math/not", "math/and", "math/or", "math/xor", "math/asr", "math/lsl", "math/clz",
            "math/ctz", "math/popcnt",
            // other pure operations
            "ref/eq", "type/boolToInt", "type/boolToFloat", "type/intToBool", "type/intToFloat",
            "type/floatToBool", "type/floatToInt", "variable/get", "pointer/get",
            // flow
            "flow/sequence", "flow/branch", "flow/switch", "flow/while", "flow/for", "flow/doN",
            "flow/multiGate", "flow/waitAll", "flow/throttle", "flow/setDelay", "flow/cancelDelay",
            "variable/set", "pointer/set", "debug/log", "variable/interpolate", "pointer/interpolate",
            // animation
            "animation/start", "animation/stop", "animation/stopAt",
            // events
            "event/onStart", "event/onTick", "event/onSelect", "event/stopPropagation", "event/receive",
            "event/send", "event/onHoverIn", "event/onHoverOut",
        };
        return ops.count(op) != 0;
    }

    // ---- values --------------------------------------------------------------------

    Sig typeAt(int64_t index) const
    {
        if (index < 0 || static_cast<size_t>(index) >= graph->types.size()) return Sig::Custom;
        return graph->types[static_cast<size_t>(index)].signature;
    }

    Value literal(int64_t typeIndex, const InteractivityValue &value) const
    {
        const Sig sig = typeAt(typeIndex);
        Value v = Value::defaultOf(sig);
        if (value.empty()) return v;
        switch (sig)
        {
        case Sig::Bool: v.b = value[0].kind == InteractivityLiteral::Kind::Bool && value[0].boolValue; break;
        case Sig::Int:
        {
            int32_t i = 0;
            if (literalInt(value[0], i)) v.i = i;
            break;
        }
        case Sig::Ref:
            if (value[0].kind == InteractivityLiteral::Kind::String) v.ref = parseObjectRef(value[0].stringValue);
            break;
        default:
            for (int k = 0; k < Value::componentCount(sig) && k < static_cast<int>(value.size()); ++k)
            {
                const auto &l = value[static_cast<size_t>(k)];
                if (l.kind == InteractivityLiteral::Kind::Number) v.f[k] = static_cast<double>(l.numberValue);
                else if (l.kind == InteractivityLiteral::Kind::Integer) v.f[k] = static_cast<double>(l.intValue);
            }
        }
        return v;
    }

    bool configInt(int64_t node, const char *key, int32_t &out) const
    {
        const auto &cfg = graph->nodes[static_cast<size_t>(node)].configuration;
        const auto it = cfg.find(key);
        return it != cfg.end() && it->second.size() == 1 && literalInt(it->second[0], out);
    }

    bool configBool(int64_t node, const char *key, bool &out) const
    {
        const auto &cfg = graph->nodes[static_cast<size_t>(node)].configuration;
        const auto it = cfg.find(key);
        if (it == cfg.end() || it->second.size() != 1 || it->second[0].kind != InteractivityLiteral::Kind::Bool)
            return false;
        out = it->second[0].boolValue;
        return true;
    }

    // An int[] configuration; false when absent or not all exact 32-bit integers.
    bool configInts(int64_t node, const char *key, std::vector<int32_t> &out) const
    {
        const auto &cfg = graph->nodes[static_cast<size_t>(node)].configuration;
        const auto it = cfg.find(key);
        if (it == cfg.end()) return false;
        out.clear();
        for (const auto &l : it->second)
        {
            int32_t i = 0;
            if (!literalInt(l, i)) return false;
            out.push_back(i);
        }
        return true;
    }

    std::string configString(int64_t node, const char *key) const
    {
        const auto &cfg = graph->nodes[static_cast<size_t>(node)].configuration;
        const auto it = cfg.find(key);
        if (it == cfg.end() || it->second.size() != 1 || it->second[0].kind != InteractivityLiteral::Kind::String)
            return {};
        return it->second[0].stringValue;
    }

    void resetState(int64_t node)
    {
        auto &s = state[static_cast<size_t>(node)];
        s = NodeState{};
        const std::string &op = ops[static_cast<size_t>(node)];
        auto &o = s.outputs;
        if (op == "flow/for")
        {
            int32_t initial = 0;
            configInt(node, "initialIndex", initial);
            o["index"] = Value::ofInt(initial);
        }
        else if (op == "flow/doN") o["currentCount"] = Value::ofInt(0);
        else if (op == "flow/multiGate")
        {
            o["lastIndex"] = Value::ofInt(-1);
            s.used.assign(flowOutputs(node).size(), false);
        }
        else if (op == "flow/waitAll")
        {
            int32_t inputs = 0;
            if (!configInt(node, "inputFlows", inputs) || inputs < 0 || inputs > 64) inputs = 0;
            o["remainingInputs"] = Value::ofInt(inputs);
            s.used.assign(static_cast<size_t>(inputs), false);
        }
        else if (op == "flow/throttle") o["lastRemainingTime"] = Value::ofFloat(kNaN);
        else if (op == "flow/setDelay") o["lastDelay"] = Value::defaultOf(Sig::Ref);
        else if (op == "event/onStart") o["event"] = Value::defaultOf(Sig::Ref);
        else if (op == "event/onTick")
        {
            o["timeSinceStart"] = Value::ofFloat(kNaN);
            o["timeSinceLastTick"] = Value::ofFloat(kNaN);
            o["event"] = Value::defaultOf(Sig::Ref);
        }
        else if (op == "event/onSelect")
        {
            o["selectedNode"] = Value::defaultOf(Sig::Ref);
            o["controllerIndex"] = Value::ofInt(-1);
            o["selectionPoint"] = Value::defaultOf(Sig::Float3);
            o["selectionRayOrigin"] = Value::defaultOf(Sig::Float3);
            o["event"] = Value::defaultOf(Sig::Ref);
        }
        else if (op == "event/onHoverIn" || op == "event/onHoverOut")
        {
            o["hoveredNode"] = Value::defaultOf(Sig::Ref);
            o["controllerIndex"] = Value::ofInt(-1);
            o["event"] = Value::defaultOf(Sig::Ref);
        }
        else if (op == "event/receive")
        {
            int32_t e = -1;
            if (configInt(node, "event", e) && e >= 0 && static_cast<size_t>(e) < graph->events.size())
                for (const auto &[id, ev] : graph->events[static_cast<size_t>(e)].values)
                    o[id] = literal(ev.type, ev.value);
            o["event"] = Value::defaultOf(Sig::Ref);
        }
    }

    // Output flow socket ids, in the specification's socket order.
    std::vector<std::string> flowOutputs(int64_t node) const
    {
        std::vector<std::string> ids;
        for (const auto &[id, flow] : graph->nodes[static_cast<size_t>(node)].flows) ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    // ---- evaluation ----------------------------------------------------------------

    Value input(int64_t node, const std::string &socket)
    {
        const auto &values = graph->nodes[static_cast<size_t>(node)].values;
        const auto it = values.find(socket);
        if (it == values.end()) return Value::defaultOf(Sig::Float);
        if (it->second.isLinked()) return output(it->second.node, it->second.socket);
        return literal(it->second.type, it->second.value);
    }

    bool hasInput(int64_t node, const std::string &socket) const
    {
        const auto &values = graph->nodes[static_cast<size_t>(node)].values;
        return values.find(socket) != values.end();
    }

    Value output(int64_t node, const std::string &socket)
    {
        if (node < 0 || static_cast<size_t>(node) >= state.size()) return Value::defaultOf(Sig::Float);
        if (flowNode[static_cast<size_t>(node)] || !supported[static_cast<size_t>(node)])
        {
            const auto &o = state[static_cast<size_t>(node)].outputs;
            const auto it = o.find(socket);
            return it != o.end() ? it->second : Value::defaultOf(Sig::Float);
        }
        auto cached = pureCache.find(node);
        if (cached == pureCache.end())
        {
            computeWithDependencies(node);
            cached = pureCache.find(node);
            if (cached == pureCache.end()) return Value::defaultOf(Sig::Float);
        }
        const auto it = cached->second.find(socket);
        return it != cached->second.end() ? it->second : Value::defaultOf(Sig::Float);
    }

    // Computes the pure node `node` and the uncached pure nodes it reads,
    // without recursion: value links always point to earlier nodes, so in
    // ascending node order every input is computed before it's read.
    void computeWithDependencies(int64_t node)
    {
        std::vector<int64_t> needed;
        std::vector<int64_t> stack{node};
        std::unordered_set<int64_t> seen{node};
        while (!stack.empty())
        {
            const int64_t n = stack.back();
            stack.pop_back();
            needed.push_back(n);
            for (const auto &[id, v] : graph->nodes[static_cast<size_t>(n)].values)
            {
                const int64_t dep = v.node;
                if (dep < 0 || static_cast<size_t>(dep) >= state.size() || seen.count(dep)) continue;
                if (flowNode[static_cast<size_t>(dep)] || !supported[static_cast<size_t>(dep)]) continue;
                if (pureCache.count(dep)) continue;
                seen.insert(dep);
                stack.push_back(dep);
            }
        }
        std::sort(needed.begin(), needed.end());
        for (const int64_t n : needed)
            if (!pureCache.count(n)) pureCache.emplace(n, computePure(n));
    }

    // ---- pure operations ---------------------------------------------------------------

    using Unary = double (*)(double);

    static Value mapFloat(const Value &a, double (*fn)(double))
    {
        Value r = a;
        for (int k = 0; k < Value::componentCount(a.type); ++k) r.f[k] = fn(a.f[k]);
        return r;
    }

    template <typename Fn> static Value zipFloat(const Value &a, const Value &b, Fn fn)
    {
        Value r = a;
        for (int k = 0; k < Value::componentCount(a.type); ++k) r.f[k] = fn(a.f[k], b.f[k]);
        return r;
    }

    template <typename Fn> static Value zip3Float(const Value &a, const Value &b, const Value &c, Fn fn)
    {
        Value r = a;
        for (int k = 0; k < Value::componentCount(a.type); ++k) r.f[k] = fn(a.f[k], b.f[k], c.f[k]);
        return r;
    }

    static double roundAway(double a) { return a < 0 ? -std::round(-a) : std::round(a); }
    static double fsign(double a) { return std::isnan(a) ? a : a > 0 ? 1.0 : a < 0 ? -1.0 : a; }
    static double fract(double a) { return a - std::floor(a); }
    static double frem(double a, double b) { return std::fmod(a, b); }
    // min/max: NaN in, NaN out; -0 < +0.
    static double fmin2(double a, double b)
    {
        if (std::isnan(a) || std::isnan(b)) return kNaN;
        if (a == b) return std::signbit(a) ? a : b;
        return a < b ? a : b;
    }
    static double fmax2(double a, double b)
    {
        if (std::isnan(a) || std::isnan(b)) return kNaN;
        if (a == b) return std::signbit(a) ? b : a;
        return a > b ? a : b;
    }
    static double saturate(double a) { return fmin2(fmax2(a, 0.0), 1.0); }

    static Value matMul(const Value &a, const Value &b)
    {
        const int n = matrixSize(a.type);
        Value r = a;
        for (int c = 0; c < n; ++c)
            for (int row = 0; row < n; ++row)
            {
                double s = 0;
                for (int k = 0; k < n; ++k) s += a.f[k * n + row] * b.f[c * n + k];
                r.f[c * n + row] = s;
            }
        return r;
    }

    static double determinant(const Value &m)
    {
        const auto &f = m.f;
        switch (matrixSize(m.type))
        {
        case 2: return f[0] * f[3] - f[2] * f[1];
        case 3:
            return f[0] * (f[4] * f[8] - f[7] * f[5]) - f[3] * (f[1] * f[8] - f[7] * f[2]) +
                   f[6] * (f[1] * f[5] - f[4] * f[2]);
        default:
        {
            // Laplace expansion along the first column (column-major: f[c*4 + r]).
            double det = 0;
            for (int r = 0; r < 4; ++r)
            {
                double minor[9];
                int k = 0;
                for (int c = 1; c < 4; ++c)
                    for (int rr = 0; rr < 4; ++rr)
                        if (rr != r) minor[k++] = f[c * 4 + rr];
                const double d3 = minor[0] * (minor[4] * minor[8] - minor[7] * minor[5]) -
                                  minor[3] * (minor[1] * minor[8] - minor[7] * minor[2]) +
                                  minor[6] * (minor[1] * minor[5] - minor[4] * minor[2]);
                det += (r % 2 ? -1.0 : 1.0) * f[r] * d3;
            }
            return det;
        }
        }
    }


    // ---- quaternions and matrices (specification, "Quaternion Operations") -------------

    using Q = std::array<double, 4>; // x, y, z, w

    static Q quatMul(const Q &a, const Q &b)
    {
        return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] + a[1] * b[3] + a[2] * b[0] - a[0] * b[2],
                a[3] * b[2] + a[2] * b[3] + a[0] * b[1] - a[1] * b[0], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    }

    static Q quatFromAxisAngle(const double *axis, double angle)
    {
        const double s = std::sin(0.5 * angle);
        return {axis[0] * s, axis[1] * s, axis[2] * s, std::cos(0.5 * angle)};
    }

    // The unit quaternion of a rotation matrix (m[row][col]).
    static Q quatFromMatrix(const double m[3][3])
    {
        const double trace = m[0][0] + m[1][1] + m[2][2];
        Q q;
        if (trace > 0)
        {
            const double t = std::sqrt(trace + 1.0) * 2;
            q = {(m[2][1] - m[1][2]) / t, (m[0][2] - m[2][0]) / t, (m[1][0] - m[0][1]) / t, 0.25 * t};
        }
        else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
        {
            const double t = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2;
            q = {0.25 * t, (m[0][1] + m[1][0]) / t, (m[0][2] + m[2][0]) / t, (m[2][1] - m[1][2]) / t};
        }
        else if (m[1][1] > m[2][2])
        {
            const double t = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2;
            q = {(m[0][1] + m[1][0]) / t, 0.25 * t, (m[1][2] + m[2][1]) / t, (m[0][2] - m[2][0]) / t};
        }
        else
        {
            const double t = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2;
            q = {(m[0][2] + m[2][0]) / t, (m[1][2] + m[2][1]) / t, 0.25 * t, (m[1][0] - m[0][1]) / t};
        }
        const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        for (double &c : q) c /= len;
        return q;
    }

    // A unit vector perpendicular to v.
    static std::array<double, 3> perpendicular(const double *v)
    {
        std::array<double, 3> axis = std::fabs(v[0]) < 0.9 ? std::array<double, 3>{1, 0, 0} : std::array<double, 3>{0, 1, 0};
        std::array<double, 3> c = {v[1] * axis[2] - v[2] * axis[1], v[2] * axis[0] - v[0] * axis[2], v[0] * axis[1] - v[1] * axis[0]};
        const double len = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
        for (double &x : c) x /= len;
        return c;
    }

    static void rotate(const double *a, const Q &r, double *out)
    {
        // a + 2 (r x (r x a) + w (r x a))
        const double rxa[3] = {r[1] * a[2] - r[2] * a[1], r[2] * a[0] - r[0] * a[2], r[0] * a[1] - r[1] * a[0]};
        const double rxrxa[3] = {r[1] * rxa[2] - r[2] * rxa[1], r[2] * rxa[0] - r[0] * rxa[2], r[0] * rxa[1] - r[1] * rxa[0]};
        for (int k = 0; k < 3; ++k) out[k] = a[k] + 2 * (rxrxa[k] + r[3] * rxa[k]);
    }

    // Inverse by Gauss-Jordan elimination (column-major in and out); false when singular.
    static bool inverse(const Value &m, Value &out)
    {
        const int n = matrixSize(m.type);
        double a[4][8] = {};
        for (int r = 0; r < n; ++r)
            for (int c = 0; c < n; ++c)
            {
                a[r][c] = m.f[c * n + r];
                a[r][n + c] = r == c ? 1.0 : 0.0;
            }
        for (int c = 0; c < n; ++c)
        {
            int pivot = c;
            for (int r = c + 1; r < n; ++r)
                if (std::fabs(a[r][c]) > std::fabs(a[pivot][c])) pivot = r;
            if (a[pivot][c] == 0 || !std::isfinite(a[pivot][c])) return false;
            if (pivot != c)
                for (int k = 0; k < 2 * n; ++k) std::swap(a[c][k], a[pivot][k]);
            const double d = a[c][c];
            for (int k = 0; k < 2 * n; ++k) a[c][k] /= d;
            for (int r = 0; r < n; ++r)
                if (r != c)
                {
                    const double f = a[r][c];
                    for (int k = 0; k < 2 * n; ++k) a[r][k] -= f * a[c][k];
                }
        }
        out = m;
        for (int r = 0; r < n; ++r)
            for (int c = 0; c < n; ++c) out.f[c * n + r] = a[r][n + c];
        return true;
    }

    std::map<std::string, Value> computePure(int64_t node)
    {
        std::map<std::string, Value> out;
        const std::string &op = ops[static_cast<size_t>(node)];
        auto in = [&](const char *s) { return input(node, s); };
        auto set = [&](const Value &v) { out["value"] = v; };

        // constants
        if (op == "math/E") { set(Value::ofFloat(2.718281828459045)); return out; }
        if (op == "math/Pi") { set(Value::ofFloat(kPi)); return out; }
        if (op == "math/Tau") { set(Value::ofFloat(2 * kPi)); return out; }
        if (op == "math/Inf") { set(Value::ofFloat(std::numeric_limits<double>::infinity())); return out; }
        if (op == "math/NaN") { set(Value::ofFloat(kNaN)); return out; }
        if (op == "math/random") { set(Value::ofFloat(host.random())); return out; }

        if (op == "variable/get")
        {
            int32_t v = -1;
            if (configInt(node, "variable", v) && v >= 0 && static_cast<size_t>(v) < variables.size())
                set(variables[static_cast<size_t>(v)]);
            return out;
        }
        if (op == "pointer/get")
        {
            int32_t t = -1;
            configInt(node, "type", t);
            const Sig sig = typeAt(t);
            Value v = Value::defaultOf(sig);
            std::string pointer;
            bool ok = effectivePointer(node, pointer);
            if (ok) ok = getPointer(pointer, sig, v);
            if (!ok) v = Value::defaultOf(sig);
            out["value"] = v;
            out["isValid"] = Value::ofBool(ok);
            return out;
        }
        if (op == "math/select")
        {
            set(in("condition").b ? in("a") : in("b"));
            return out;
        }
        if (op == "math/switch")
        {
            const int32_t sel = in("selection").i;
            std::vector<int32_t> cases;
            if (!configInts(node, "cases", cases)) cases.clear();
            if (std::find(cases.begin(), cases.end(), sel) != cases.end()) set(in(std::to_string(sel).c_str()));
            else set(in("default"));
            return out;
        }
        if (op == "ref/eq") { set(Value::ofBool(in("a").ref == in("b").ref)); return out; }

        // type conversions
        if (op == "type/boolToInt") { set(Value::ofInt(in("a").b ? 1 : 0)); return out; }
        if (op == "type/boolToFloat") { set(Value::ofFloat(in("a").b ? 1.0 : 0.0)); return out; }
        if (op == "type/intToBool") { set(Value::ofBool(in("a").i != 0)); return out; }
        if (op == "type/intToFloat") { set(Value::ofFloat(static_cast<double>(in("a").i))); return out; }
        if (op == "type/floatToBool")
        {
            const double a = in("a").f[0];
            set(Value::ofBool(!(a == 0.0 || std::isnan(a))));
            return out;
        }
        if (op == "type/floatToInt")
        {
            const double a = in("a").f[0];
            // Truncate; NaN and infinities give 0; others wrap modulo 2^32.
            int32_t r = 0;
            if (std::isfinite(a)) r = wrap(static_cast<int64_t>(std::fmod(std::trunc(a), 4294967296.0)));
            set(Value::ofInt(r));
            return out;
        }

        // swizzles
        if (op == "math/combine2" || op == "math/combine3" || op == "math/combine4" || op == "math/combine2x2" ||
            op == "math/combine3x3" || op == "math/combine4x4")
        {
            const Sig sig = op == "math/combine2"     ? Sig::Float2
                            : op == "math/combine3"   ? Sig::Float3
                            : op == "math/combine4"   ? Sig::Float4
                            : op == "math/combine2x2" ? Sig::Float2x2
                            : op == "math/combine3x3" ? Sig::Float3x3
                                                      : Sig::Float4x4;
            Value v = Value::defaultOf(sig);
            for (int k = 0; k < Value::componentCount(sig); ++k)
            {
                const char id[2] = {static_cast<char>('a' + k), 0};
                v.f[k] = input(node, id).f[0];
            }
            set(v);
            return out;
        }
        if (op.rfind("math/extract", 0) == 0)
        {
            const Value a = in("a");
            for (int k = 0; k < Value::componentCount(a.type); ++k) out[std::to_string(k)] = Value::ofFloat(a.f[k]);
            return out;
        }

        // integer / boolean bitwise
        if (op == "math/not")
        {
            const Value a = in("a");
            set(a.type == Sig::Bool ? Value::ofBool(!a.b) : Value::ofInt(~a.i));
            return out;
        }
        if (op == "math/and" || op == "math/or" || op == "math/xor")
        {
            const Value a = in("a"), b = in("b");
            if (a.type == Sig::Bool)
                set(Value::ofBool(op == "math/and" ? (a.b && b.b) : op == "math/or" ? (a.b || b.b) : (a.b != b.b)));
            else
                set(Value::ofInt(op == "math/and" ? (a.i & b.i) : op == "math/or" ? (a.i | b.i) : (a.i ^ b.i)));
            return out;
        }
        if (op == "math/asr" || op == "math/lsl")
        {
            const int32_t a = in("a").i;
            const uint32_t s = static_cast<uint32_t>(in("b").i) & 31u;
            set(Value::ofInt(op == "math/asr" ? (a >> s) : wrap(static_cast<int64_t>(static_cast<uint32_t>(a) << s))));
            return out;
        }
        if (op == "math/clz" || op == "math/ctz" || op == "math/popcnt")
        {
            const uint32_t a = static_cast<uint32_t>(in("a").i);
            set(Value::ofInt(op == "math/clz"   ? std::countl_zero(a)
                             : op == "math/ctz" ? std::countr_zero(a)
                                                : std::popcount(a)));
            return out;
        }

        // comparisons
        if (op == "math/eq")
        {
            const Value a = in("a"), b = in("b");
            bool r = true;
            if (a.type == Sig::Bool) r = a.b == b.b;
            else if (a.type == Sig::Int) r = a.i == b.i;
            else
                for (int k = 0; k < Value::componentCount(a.type); ++k) r = r && a.f[k] == b.f[k];
            set(Value::ofBool(r));
            return out;
        }
        if (op == "math/lt" || op == "math/le" || op == "math/gt" || op == "math/ge")
        {
            const Value a = in("a"), b = in("b");
            bool r;
            if (a.type == Sig::Int)
                r = op == "math/lt" ? a.i < b.i : op == "math/le" ? a.i <= b.i : op == "math/gt" ? a.i > b.i : a.i >= b.i;
            else
            {
                const double x = a.f[0], y = b.f[0];
                r = op == "math/lt" ? x < y : op == "math/le" ? x <= y : op == "math/gt" ? x > y : x >= y;
            }
            set(Value::ofBool(r));
            return out;
        }
        if (op == "math/isNaN") { set(Value::ofBool(std::isnan(in("a").f[0]))); return out; }
        if (op == "math/isInf") { set(Value::ofBool(std::isinf(in("a").f[0]))); return out; }

        // vectors and matrices
        if (op == "math/length" || op == "math/normalize" || op == "math/dot")
        {
            const Value a = in("a");
            const int n = Value::componentCount(a.type);
            if (op == "math/dot")
            {
                const Value b = in("b");
                double s = 0;
                for (int k = 0; k < n; ++k) s += a.f[k] * b.f[k];
                set(Value::ofFloat(s));
                return out;
            }
            double len = 0;
            for (int k = 0; k < n; ++k) len += a.f[k] * a.f[k];
            len = std::sqrt(len);
            if (op == "math/length") { set(Value::ofFloat(len)); return out; }
            const bool ok = len > 0 && std::isfinite(len);
            Value v = a;
            for (int k = 0; k < n; ++k) v.f[k] = ok ? a.f[k] / len : 0.0;
            set(v);
            out["isValid"] = Value::ofBool(ok);
            return out;
        }
        if (op == "math/cross")
        {
            const Value a = in("a"), b = in("b");
            const double r[3] = {a.f[1] * b.f[2] - a.f[2] * b.f[1], a.f[2] * b.f[0] - a.f[0] * b.f[2],
                                 a.f[0] * b.f[1] - a.f[1] * b.f[0]};
            set(Value::ofFloats(Sig::Float3, r));
            return out;
        }
        if (op == "math/rotate2D")
        {
            const Value a = in("a");
            const double t = in("angle").f[0];
            const double r[2] = {a.f[0] * std::cos(t) - a.f[1] * std::sin(t), a.f[0] * std::sin(t) + a.f[1] * std::cos(t)};
            set(Value::ofFloats(Sig::Float2, r));
            return out;
        }
        if (op == "math/transform")
        {
            const Value a = in("a"), m = in("b");
            const int n = Value::componentCount(a.type);
            Value v = a;
            for (int row = 0; row < n; ++row)
            {
                double s = 0;
                for (int c = 0; c < n; ++c) s += m.f[c * n + row] * a.f[c];
                v.f[row] = s;
            }
            set(v);
            return out;
        }
        if (op == "math/transpose")
        {
            const Value a = in("a");
            const int n = matrixSize(a.type);
            Value v = a;
            for (int c = 0; c < n; ++c)
                for (int r = 0; r < n; ++r) v.f[c * n + r] = a.f[r * n + c];
            set(v);
            return out;
        }
        if (op == "math/determinant") { set(Value::ofFloat(determinant(in("a")))); return out; }
        if (op == "math/matMul") { set(matMul(in("a"), in("b"))); return out; }

        if (op == "math/inverse")
        {
            const Value a = in("a");
            const double det = determinant(a);
            Value v = a;
            bool ok = det != 0 && std::isfinite(det) && inverse(a, v);
            if (!ok)
            {
                v = Value::defaultOf(a.type);
                for (int k = 0; k < Value::componentCount(a.type); ++k) v.f[k] = 0.0;
            }
            set(v);
            out["isValid"] = Value::ofBool(ok);
            return out;
        }
        if (op == "math/matCompose")
        {
            const Value t = in("translation"), r = in("rotation"), sc = in("scale");
            const double x = r.f[0], y = r.f[1], z = r.f[2], w = r.f[3];
            const double m[16] = {sc.f[0] * (1 - 2 * (y * y + z * z)), sc.f[0] * 2 * (x * y + z * w), sc.f[0] * 2 * (x * z - y * w), 0,
                                  sc.f[1] * 2 * (x * y - z * w), sc.f[1] * (1 - 2 * (x * x + z * z)), sc.f[1] * 2 * (y * z + x * w), 0,
                                  sc.f[2] * 2 * (x * z + y * w), sc.f[2] * 2 * (y * z - x * w), sc.f[2] * (1 - 2 * (x * x + y * y)), 0,
                                  t.f[0], t.f[1], t.f[2], 1};
            set(Value::ofFloats(Sig::Float4x4, m));
            return out;
        }
        if (op == "math/matDecompose")
        {
            const Value a = in("a");
            const double t[3] = {a.f[12], a.f[13], a.f[14]};
            double sc[3];
            for (int c = 0; c < 3; ++c)
                sc[c] = std::sqrt(a.f[c * 4] * a.f[c * 4] + a.f[c * 4 + 1] * a.f[c * 4 + 1] + a.f[c * 4 + 2] * a.f[c * 4 + 2]);
            const double identity[4] = {0, 0, 0, 1};
            out["translation"] = Value::ofFloats(Sig::Float3, t);
            bool degenerate = false;
            for (const double v : sc) degenerate = degenerate || !std::isfinite(v) || v == 0;
            if (degenerate)
            {
                out["scale"] = Value::ofFloats(Sig::Float3, sc);
                out["rotation"] = Value::ofFloats(Sig::Float4, identity);
                return out;
            }
            double b[3][3];
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c) b[r][c] = a.f[c * 4 + r] / sc[c];
            const double det = b[0][0] * (b[1][1] * b[2][2] - b[2][1] * b[1][2]) - b[0][1] * (b[1][0] * b[2][2] - b[2][0] * b[1][2]) +
                               b[0][2] * (b[1][0] * b[2][1] - b[2][0] * b[1][1]);
            if (std::fabs(std::fabs(det) - 1.0) > 1e-3)
            {
                // Shear: not decomposable; NaN rotation and scale.
                out["scale"] = Value::defaultOf(Sig::Float3);
                out["rotation"] = Value::defaultOf(Sig::Float4);
                return out;
            }
            if (det < 0)
            {
                sc[0] = -sc[0];
                for (int r = 0; r < 3; ++r) b[r][0] = -b[r][0];
            }
            out["scale"] = Value::ofFloats(Sig::Float3, sc);
            const Q q = quatFromMatrix(b);
            out["rotation"] = Value::ofFloats(Sig::Float4, q.data());
            return out;
        }
        if (op == "math/rotate3D")
        {
            const Value a = in("a"), r = in("rotation");
            double v[3];
            rotate(a.f.data(), Q{r.f[0], r.f[1], r.f[2], r.f[3]}, v);
            set(Value::ofFloats(Sig::Float3, v));
            return out;
        }
        if (op == "math/slerp")
        {
            const Value a = in("a"), b = in("b");
            const double c = in("c").f[0];
            const int n = Value::componentCount(a.type);
            double la = 0, lb = 0;
            for (int k = 0; k < n; ++k) { la += a.f[k] * a.f[k]; lb += b.f[k] * b.f[k]; }
            la = std::sqrt(la);
            lb = std::sqrt(lb);
            Value v = a;
            const auto lerp = [&] { for (int k = 0; k < n; ++k) v.f[k] = (1 - c) * a.f[k] + c * b.f[k]; };
            if (la < 1e-9 || lb < 1e-9) { lerp(); set(v); return out; }
            double ah[3] = {}, bh[3] = {};
            for (int k = 0; k < n; ++k) { ah[k] = a.f[k] / la; bh[k] = b.f[k] / lb; }
            const double L = (1 - c) * la + c * lb;
            if (n == 2)
            {
                double theta = std::acos(std::clamp(ah[0] * bh[0] + ah[1] * bh[1], -1.0, 1.0));
                if (ah[0] * bh[1] - ah[1] * bh[0] < 0) theta = -theta;
                v.f[0] = (ah[0] * std::cos(c * theta) - ah[1] * std::sin(c * theta)) * L;
                v.f[1] = (ah[0] * std::sin(c * theta) + ah[1] * std::cos(c * theta)) * L;
                set(v);
                return out;
            }
            const double d = ah[0] * bh[0] + ah[1] * bh[1] + ah[2] * bh[2];
            if (d > 1 - 1e-9) { lerp(); set(v); return out; }
            std::array<double, 3> r;
            if (d < -1 + 1e-9) r = perpendicular(ah);
            else
            {
                r = {ah[1] * bh[2] - ah[2] * bh[1], ah[2] * bh[0] - ah[0] * bh[2], ah[0] * bh[1] - ah[1] * bh[0]};
                const double len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
                for (double &x : r) x /= len;
            }
            const Q q = quatFromAxisAngle(r.data(), c * std::acos(std::clamp(d, -1.0, 1.0)));
            double rotated[3];
            rotate(ah, q, rotated);
            for (int k = 0; k < 3; ++k) v.f[k] = rotated[k] * L;
            set(v);
            return out;
        }
        if (op.rfind("math/quat", 0) == 0)
        {
            const auto q4 = [&](const char *id) { const Value v = in(id); return Q{v.f[0], v.f[1], v.f[2], v.f[3]}; };
            if (op == "math/quatConjugate")
            {
                const Q a = q4("a");
                const Q r = {-a[0], -a[1], -a[2], a[3]};
                set(Value::ofFloats(Sig::Float4, r.data()));
            }
            else if (op == "math/quatMul") set(Value::ofFloats(Sig::Float4, quatMul(q4("a"), q4("b")).data()));
            else if (op == "math/quatAngleBetween")
            {
                const Q a = q4("a"), b = q4("b");
                set(Value::ofFloat(2 * std::acos(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3])));
            }
            else if (op == "math/quatFromAxisAngle")
            {
                const Value axis = in("axis");
                set(Value::ofFloats(Sig::Float4, quatFromAxisAngle(axis.f.data(), in("angle").f[0]).data()));
            }
            else if (op == "math/quatToAxisAngle")
            {
                const Q a = q4("a");
                double axis[3] = {1, 0, 0};
                double angle = 0;
                if (std::fabs(a[3]) < 1 - 1e-9)
                {
                    const double k = std::sqrt(1 - a[3] * a[3]);
                    axis[0] = a[0] / k;
                    axis[1] = a[1] / k;
                    axis[2] = a[2] / k;
                    angle = 2 * std::acos(a[3]);
                }
                else if (std::isnan(a[3]))
                {
                    axis[0] = axis[1] = axis[2] = angle = kNaN;
                }
                out["axis"] = Value::ofFloats(Sig::Float3, axis);
                out["angle"] = Value::ofFloat(angle);
            }
            else if (op == "math/quatFromDirections")
            {
                const Value av = in("a"), bv = in("b");
                const double *a = av.f.data(), *b = bv.f.data();
                const double c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
                Q q = {0, 0, 0, 1};
                if (c < -1 + 1e-9)
                {
                    const auto p = perpendicular(a);
                    q = {p[0], p[1], p[2], 0};
                }
                else if (c <= 1 - 1e-9)
                {
                    double r[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
                    const double len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
                    const double k = std::sqrt(0.5 - 0.5 * c);
                    q = {r[0] / len * k, r[1] / len * k, r[2] / len * k, std::sqrt(0.5 + 0.5 * c)};
                }
                else if (std::isnan(c)) q = {kNaN, kNaN, kNaN, kNaN};
                set(Value::ofFloats(Sig::Float4, q.data()));
            }
            else if (op == "math/quatFromUpForward")
            {
                const Value up = in("up"), fwd = in("forward");
                const double *y = up.f.data(), *r = fwd.f.data();
                double sv[3] = {y[1] * r[2] - y[2] * r[1], y[2] * r[0] - y[0] * r[2], y[0] * r[1] - y[1] * r[0]};
                double len = std::sqrt(sv[0] * sv[0] + sv[1] * sv[1] + sv[2] * sv[2]);
                if (len < 1e-9)
                {
                    const auto p = perpendicular(r);
                    sv[0] = p[0]; sv[1] = p[1]; sv[2] = p[2];
                }
                else
                    for (double &x : sv) x /= len;
                const double t[3] = {r[1] * sv[2] - r[2] * sv[1], r[2] * sv[0] - r[0] * sv[2], r[0] * sv[1] - r[1] * sv[0]};
                const double m[3][3] = {{sv[0], t[0], r[0]}, {sv[1], t[1], r[1]}, {sv[2], t[2], r[2]}};
                set(Value::ofFloats(Sig::Float4, quatFromMatrix(m).data()));
            }
            else if (op == "math/quatFromAngles")
            {
                std::string order = configString(node, "order");
                static const std::set<std::string> orders = {"xyz", "xzy", "yxz", "yzx", "zxy", "zyx"};
                if (!orders.count(order)) order = "yxz";
                const double angles[3] = {in("x").f[0], in("y").f[0], in("z").f[0]};
                // Intrinsic: rotate about the first axis, then the second (rotated) one...
                Q q = {0, 0, 0, 1};
                for (const char axisName : order)
                {
                    const int k = axisName - 'x';
                    double axis[3] = {0, 0, 0};
                    axis[k] = 1;
                    q = quatMul(q, quatFromAxisAngle(axis, angles[k]));
                }
                set(Value::ofFloats(Sig::Float4, q.data()));
            }
            else if (op == "math/quatSlerp")
            {
                const Q a = q4("a");
                Q b = q4("b");
                const double c = in("c").f[0];
                double d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
                if (d < 0)
                {
                    d = -d;
                    for (double &x : b) x = -x;
                }
                double ka = 1 - c, kb = c;
                if (d < 1 - 1e-9)
                {
                    const double w = std::acos(d);
                    ka = std::sin(w * (1 - c)) / std::sin(w);
                    kb = std::sin(w * c) / std::sin(w);
                }
                const Q r = {a[0] * ka + b[0] * kb, a[1] * ka + b[1] * kb, a[2] * ka + b[2] * kb, a[3] * ka + b[3] * kb};
                set(Value::ofFloats(Sig::Float4, r.data()));
            }
            return out;
        }

        // unary component-wise
        static const std::unordered_map<std::string, Unary> unary = {
            {"math/trunc", [](double a) { return std::trunc(a); }},
            {"math/floor", [](double a) { return std::floor(a); }},
            {"math/ceil", [](double a) { return std::ceil(a); }},
            {"math/round", &roundAway},
            {"math/fract", &fract},
            {"math/saturate", &saturate},
            {"math/rad", [](double a) { return a * kPi / 180.0; }},
            {"math/deg", [](double a) { return a * 180.0 / kPi; }},
            {"math/sin", [](double a) { return std::sin(a); }},
            {"math/cos", [](double a) { return std::cos(a); }},
            {"math/tan", [](double a) { return std::tan(a); }},
            {"math/asin", [](double a) { return std::asin(a); }},
            {"math/acos", [](double a) { return std::acos(a); }},
            {"math/atan", [](double a) { return std::atan(a); }},
            {"math/sinh", [](double a) { return std::sinh(a); }},
            {"math/cosh", [](double a) { return std::cosh(a); }},
            {"math/tanh", [](double a) { return std::tanh(a); }},
            {"math/asinh", [](double a) { return std::asinh(a); }},
            {"math/acosh", [](double a) { return std::acosh(a); }},
            {"math/atanh", [](double a) { return std::atanh(a); }},
            {"math/exp", [](double a) { return std::exp(a); }},
            {"math/log", [](double a) { return std::log(a); }},
            {"math/log2", [](double a) { return std::log2(a); }},
            {"math/log10", [](double a) { return std::log10(a); }},
            {"math/sqrt", [](double a) { return std::sqrt(a); }},
            {"math/cbrt", [](double a) { return std::cbrt(a); }},
        };
        if (const auto it = unary.find(op); it != unary.end()) { set(mapFloat(in("a"), it->second)); return out; }

        // arithmetic with int overloads
        if (op == "math/abs" || op == "math/sign" || op == "math/neg")
        {
            const Value a = in("a");
            if (a.type == Sig::Int)
            {
                const int64_t x = a.i;
                set(Value::ofInt(op == "math/abs"    ? wrap(x < 0 ? -x : x)
                                 : op == "math/sign" ? (x > 0) - (x < 0)
                                                     : wrap(-x)));
            }
            else
                set(mapFloat(a, op == "math/abs" ? static_cast<Unary>([](double v) { return std::fabs(v); })
                                : op == "math/sign" ? &fsign
                                                    : static_cast<Unary>([](double v) { return -v; })));
            return out;
        }
        if (op == "math/add" || op == "math/sub" || op == "math/mul" || op == "math/div" || op == "math/rem" ||
            op == "math/min" || op == "math/max")
        {
            const Value a = in("a"), b = in("b");
            if (a.type == Sig::Int)
            {
                const int64_t x = a.i, y = b.i;
                int32_t r = 0;
                if (op == "math/add") r = wrap(x + y);
                else if (op == "math/sub") r = wrap(x - y);
                else if (op == "math/mul") r = wrap(x * y);
                else if (op == "math/div") r = y == 0 ? 0 : wrap(x / y);
                else if (op == "math/rem") r = y == 0 ? 0 : wrap(x - y * (x / y));
                else if (op == "math/min") r = static_cast<int32_t>(std::min(x, y));
                else r = static_cast<int32_t>(std::max(x, y));
                set(Value::ofInt(r));
                return out;
            }
            // floatNxN * floatNxN is component-wise (math/matMul is the matrix product).
            if (op == "math/add") set(zipFloat(a, b, [](double x, double y) { return x + y; }));
            else if (op == "math/sub") set(zipFloat(a, b, [](double x, double y) { return x - y; }));
            else if (op == "math/mul") set(zipFloat(a, b, [](double x, double y) { return x * y; }));
            else if (op == "math/div") set(zipFloat(a, b, [](double x, double y) { return x / y; }));
            else if (op == "math/rem") set(zipFloat(a, b, &frem));
            else if (op == "math/min") set(zipFloat(a, b, &fmin2));
            else set(zipFloat(a, b, &fmax2));
            return out;
        }
        if (op == "math/atan2") { set(zipFloat(in("a"), in("b"), [](double y, double x) { return std::atan2(y, x); })); return out; }
        if (op == "math/pow") { set(zipFloat(in("a"), in("b"), [](double x, double y) { return std::pow(x, y); })); return out; }
        if (op == "math/clamp")
        {
            const Value a = in("a"), b = in("b"), c = in("c");
            if (a.type == Sig::Int)
            {
                const int32_t lo = std::min(b.i, c.i), hi = std::max(b.i, c.i);
                set(Value::ofInt(std::min(std::max(a.i, lo), hi)));
            }
            else
                set(zip3Float(a, b, c, [](double x, double y, double z) { return fmin2(fmax2(x, fmin2(y, z)), fmax2(y, z)); }));
            return out;
        }
        if (op == "math/mix")
        {
            set(zip3Float(in("a"), in("b"), in("c"), [](double x, double y, double t) { return x + t * (y - x); }));
            return out;
        }
        if (op == "math/smoothStep")
        {
            set(zip3Float(in("a"), in("b"), in("c"), [](double x, double y, double c) {
                const double t = saturate((c - fmin2(x, y)) / std::fabs(y - x));
                return t * t * (3 - 2 * t);
            }));
            return out;
        }
        return out; // not implemented: no outputs (type defaults)
    }

    // ---- pointers --------------------------------------------------------------------

    bool effectivePointer(int64_t node, std::string &pointer)
    {
        const PointerTemplate &t = pointers[static_cast<size_t>(node)];
        if (!t.valid) return false;
        std::string p = t.text;
        for (auto it = t.params.rbegin(); it != t.params.rend(); ++it)
        {
            const Value v = input(node, it->id);
            std::string text;
            if (it->reference)
            {
                if (v.ref.isNull()) return false;
                if (v.ref.kind == Ref::Kind::Object) text = std::to_string(v.ref.index);
                else text = "@" + std::to_string(v.ref.id); // event and delay references
            }
            else
            {
                if (v.i < 0) return false;
                text = std::to_string(v.i);
            }
            p.replace(it->start, it->length, text);
        }
        pointer = unescapeBrackets(p);
        return true;
    }

    // ---- animations and interpolation ---------------------------------------------------

    // Animation State: an animation is valid when its channels' samplers and
    // their input accessors exist, with the min and max glTF requires there.
    void initAnimations()
    {
        if (!asset.animations) return;
        animations.resize(asset.animations->size());
        for (size_t i = 0; i < asset.animations->size(); ++i)
        {
            const auto &anim = (*asset.animations)[i];
            auto &info = animations[i];
            double lo = std::numeric_limits<double>::infinity(), hi = 0;
            bool valid = true;
            for (const auto &channel : anim.channels)
            {
                if (channel.sampler >= anim.samplers.size()) { valid = false; break; }
                const uint64_t input = anim.samplers[channel.sampler].input;
                if (!asset.accessors || input >= asset.accessors->size()) { valid = false; break; }
                const auto &acc = (*asset.accessors)[input];
                if (!acc.min || !acc.max || acc.min->empty() || acc.max->empty()) { valid = false; break; }
                lo = std::min(lo, static_cast<double>((*acc.min)[0]));
                hi = std::max(hi, static_cast<double>((*acc.max)[0]));
            }
            info.valid = valid;
            if (!valid) continue;
            info.minTime = std::isfinite(lo) ? lo : 0;
            info.maxTime = hi;
            info.playhead = info.virtualPlayhead = 0;
        }
    }

    // The number of objects in a top-level glTF array (0 when unknown).
    int64_t collectionSize(const std::string &name) const
    {
        auto size = [](const auto &array) -> int64_t { return array ? static_cast<int64_t>(array->size()) : 0; };
        if (name == "nodes") return size(asset.nodes);
        if (name == "meshes") return size(asset.meshes);
        if (name == "materials") return size(asset.materials);
        if (name == "animations") return size(asset.animations);
        if (name == "cameras") return size(asset.cameras);
        if (name == "skins") return size(asset.skins);
        if (name == "textures") return size(asset.textures);
        if (name == "images") return size(asset.images);
        if (name == "samplers") return size(asset.samplers);
        if (name == "scenes") return size(asset.scenes);
        if (name == "accessors") return size(asset.accessors);
        return 0;
    }

    // A ref to a glTF animation: its index, or -1.
    int64_t animationIndex(const Value &v) const
    {
        if (v.ref.kind != Ref::Kind::Object || v.ref.collection != "animations") return -1;
        if (v.ref.index < 0 || static_cast<size_t>(v.ref.index) >= animations.size()) return -1;
        return v.ref.index;
    }

    // The specification's requested time r -> effective time in [0, T].
    static double effectiveTime(double r, double T)
    {
        if (!(T > 0)) return 0;
        const double s = r > 0 ? std::ceil((r - T) / T) : std::floor(r / T);
        return r - s * T;
    }

    // The easing: the cubic Bézier from (0,0) to (1,1) through p1, p2, as
    // y(x(u)⁻¹(t)). x(u) is monotonic (x control points are in [0, 1]).
    static double ease(const double *p1, const double *p2, double t)
    {
        auto bezier = [](double a, double b, double u) {
            const double v = 1 - u;
            return 3 * v * v * u * a + 3 * v * u * u * b + u * u * u;
        };
        double lo = 0, hi = 1, u = t;
        for (int k = 0; k < 60; ++k)
        {
            u = 0.5 * (lo + hi);
            if (bezier(p1[0], p2[0], u) < t) lo = u;
            else hi = u;
        }
        return bezier(p1[1], p2[1], u);
    }

    static Value mix(const Value &a, const Value &b, double q, bool slerp)
    {
        Value v = b;
        if (slerp && b.type == Sig::Float4)
        {
            double d = a.f[0] * b.f[0] + a.f[1] * b.f[1] + a.f[2] * b.f[2] + a.f[3] * b.f[3];
            double sign = 1;
            if (d < 0) { d = -d; sign = -1; }
            double ka = 1 - q, kb = q;
            if (d < 1 - 1e-9)
            {
                const double w = std::acos(d);
                ka = std::sin(w * (1 - q)) / std::sin(w);
                kb = std::sin(w * q) / std::sin(w);
            }
            for (int k = 0; k < 4; ++k) v.f[k] = a.f[k] * ka + sign * b.f[k] * kb;
            return v;
        }
        for (int k = 0; k < Value::componentCount(b.type); ++k) v.f[k] = a.f[k] + (b.f[k] - a.f[k]) * q;
        return v;
    }

    // duration and the control points of an interpolation are usable.
    bool validEasing(int64_t node, double &duration, double *p1, double *p2)
    {
        duration = input(node, "duration").f[0];
        if (!std::isfinite(duration) || duration < 0 || duration > kMaxDuration) return false;
        const Value a = input(node, "p1"), b = input(node, "p2");
        for (int k = 0; k < 2; ++k)
        {
            if (!std::isfinite(a.f[k]) || !std::isfinite(b.f[k])) return false;
            p1[k] = a.f[k];
            p2[k] = b.f[k];
        }
        return p1[0] >= 0 && p1[0] <= 1 && p2[0] >= 0 && p2[0] <= 1;
    }

    // Runs a `done` flow from the tick (a nested activation from depth 0).
    void fireDone(int64_t node)
    {
        if (node < 0) return;
        steps = 0;
        aborted = false;
        depth = 0;
        fire(node, "done");
    }

    // Each animation the graph plays, at the current time (animation/start's
    // "asset animation update").
    void updateAnimations()
    {
        std::vector<uint64_t> ids;
        for (const auto &p : playbacks) ids.push_back(p.id);
        for (const uint64_t id : ids)
        {
            const auto it = std::find_if(playbacks.begin(), playbacks.end(), [id](const Playback &p) { return p.id == id; });
            if (it == playbacks.end()) continue; // a `done` handler stopped it
            const Playback p = *it;
            double r;
            bool finished = true;
            int64_t done = -1;
            if (p.startTime == p.endTime)
            {
                r = p.startTime;
                done = p.endNode;
            }
            else
            {
                const bool forward = p.startTime < p.endTime;
                double scaled = std::max(0.0, time - p.created) * p.speed;
                if (!forward) scaled = -scaled;
                const double current = p.startTime + scaled;
                if (forward ? (current >= p.stopTime && p.stopTime >= p.startTime && p.stopTime < p.endTime)
                            : (current <= p.stopTime && p.stopTime <= p.startTime && p.stopTime > p.endTime))
                {
                    r = p.stopTime;
                    done = p.stopNode;
                }
                else if (forward ? current >= p.endTime : current <= p.endTime)
                {
                    r = p.endTime;
                    done = p.endNode;
                }
                else
                {
                    r = current;
                    finished = false;
                }
            }
            auto &info = animations[static_cast<size_t>(p.animation)];
            info.virtualPlayhead = r;
            info.playhead = effectiveTime(r, info.maxTime);
            host.applyAnimation(p.animation, info.playhead);
            if (!finished) continue;
            playbacks.erase(std::find_if(playbacks.begin(), playbacks.end(), [id](const Playback &q) { return q.id == id; }));
            host.animationStopped(p.animation);
            fireDone(done);
        }
    }

    // Each variable/pointer interpolation in progress, at the current time.
    void updateInterpolations()
    {
        std::vector<uint64_t> ids;
        for (const auto &e : interpolations) ids.push_back(e.id);
        for (const uint64_t id : ids)
        {
            const auto it = std::find_if(interpolations.begin(), interpolations.end(),
                                         [id](const Interpolation &e) { return e.id == id; });
            if (it == interpolations.end()) continue;
            const Interpolation e = *it;
            const double t = (time - e.start) / e.duration;
            if (t <= 0) continue;
            const bool finished = std::isnan(t) || t >= 1;
            Value v = finished ? e.to : mix(e.from, e.to, ease(e.p1, e.p2, t), e.slerp);
            if (e.variable >= 0) variables[static_cast<size_t>(e.variable)] = v;
            else host.setPointer(e.pointer, v);
            if (!finished) continue;
            interpolations.erase(std::find_if(interpolations.begin(), interpolations.end(),
                                              [id](const Interpolation &x) { return x.id == id; }));
            fireDone(e.node);
        }
    }

    bool getPointer(const std::string &pointer, Sig sig, Value &v)
    {
        // Animation State: /animations/N/extensions/KHR_interactivity/<property>.
        if (pointer.rfind("/animations/", 0) == 0)
        {
            const std::string rest = pointer.substr(12);
            const size_t slash = rest.find('/');
            const std::string suffix = slash == std::string::npos ? "" : rest.substr(slash);
            const std::string index = rest.substr(0, slash);
            if (suffix.rfind("/extensions/KHR_interactivity/", 0) == 0 && !index.empty() && index.size() < 10 &&
                index.find_first_not_of("0123456789") == std::string::npos)
            {
                const size_t a = std::stoul(index);
                if (a >= animations.size()) return false;
                const auto &info = animations[a];
                const std::string property = suffix.substr(30);
                if (property == "isPlaying")
                {
                    if (sig != Sig::Bool) return false;
                    v = Value::ofBool(std::any_of(playbacks.begin(), playbacks.end(),
                                                  [a](const Playback &p) { return p.animation == static_cast<int64_t>(a); }));
                    return true;
                }
                const double *f = property == "minTime" ? &info.minTime
                                  : property == "maxTime" ? &info.maxTime
                                  : property == "playhead" ? &info.playhead
                                  : property == "virtualPlayhead" ? &info.virtualPlayhead
                                  : nullptr;
                if (!f || sig != Sig::Float) return false;
                v = Value::ofFloat(*f);
                return true;
            }
        }
        // A glTF object itself ("/animations/1" as a ref).
        if (sig == Sig::Ref)
        {
            const Ref r = parseObjectRef(pointer);
            if (r.kind == Ref::Kind::Object && r.index < collectionSize(r.collection))
            {
                v = Value::ofRef(r);
                return true;
            }
        }
        // Implementation limits.
        if (pointer.rfind("/extensions/KHR_interactivity/limits/", 0) == 0)
        {
            const std::string name = pointer.substr(37);
            if (sig != Sig::Int) return false;
            if (name == "maxActiveDelays" || name == "maxActiveAnimations" ||
                name == "maxActivePropertyInterpolations" || name == "maxActiveVariableInterpolations")
            {
                v = Value::ofInt(static_cast<int32_t>(kMaxActive));
                return true;
            }
            return false;
        }
        // The runtime's own read-only properties.
        for (const char *prefix : {"/extensions/KHR_interactivity/delays/@", "/extensions/KHR_interactivity/events/@"})
        {
            const std::string pre(prefix);
            if (pointer.rfind(pre, 0) != 0) continue;
            if (sig != Sig::Ref) return false;
            const uint64_t id = std::strtoull(pointer.c_str() + pre.size(), nullptr, 10);
            const bool delay = pre.find("delays") != std::string::npos;
            if (delay)
            {
                for (const auto &d : delays)
                    if (d.id == id) { v.ref = Ref{Ref::Kind::Delay, {}, -1, id}; return true; }
                return false;
            }
            if (id == 0 || id >= nextId) return false;
            v.ref = Ref{Ref::Kind::Event, {}, -1, id};
            return true;
        }
        return host.getPointer(pointer, sig, v);
    }

    // ---- flow ----------------------------------------------------------------------------

    void abort(const std::string &why)
    {
        if (!aborted) errors.push_back("KHR_interactivity: " + why + "; the event was stopped");
        aborted = true;
    }

    // Activates an output flow and returns when what it started is done
    // (a nested activation: loop bodies, a sequence's outputs but the last).
    void fire(int64_t node, const std::string &socket)
    {
        if (aborted) return;
        const auto &flows = graph->nodes[static_cast<size_t>(node)].flows;
        const auto it = flows.find(socket);
        if (it == flows.end() || it->second.node < 0) return;
        activate(it->second.node, it->second.socket);
    }

    // An operation's last output flow activation: run by activate()'s loop
    // after the operation returns, so long chains of flow nodes don't nest.
    // Must be the operation's last action.
    void tailFire(int64_t node, const std::string &socket)
    {
        if (aborted) return;
        const auto &flows = graph->nodes[static_cast<size_t>(node)].flows;
        const auto it = flows.find(socket);
        if (it == flows.end() || it->second.node < 0) return;
        nextNode = it->second.node;
        nextSocket = it->second.socket;
    }

    void activate(int64_t node, std::string socket)
    {
        if (aborted) return;
        if (++depth > kMaxDepth) { abort("flows nested deeper than " + std::to_string(kMaxDepth)); --depth; return; }
        while (node >= 0 && !aborted)
        {
            if (++steps > kStepBudget) { abort("more than " + std::to_string(kStepBudget) + " steps"); break; }
            // Pure nodes' outputs are kept until a flow node runs.
            pureCache.clear();
            nextNode = -1;
            if (supported[static_cast<size_t>(node)]) run(node, socket);
            node = nextNode;
            socket = nextSocket;
            nextNode = -1;
        }
        --depth;
    }

    void run(int64_t node, const std::string &socket)
    {
        const std::string &op = ops[static_cast<size_t>(node)];
        auto &s = state[static_cast<size_t>(node)];
        auto &o = s.outputs;

        if (op == "flow/sequence")
        {
            const auto ids = flowOutputs(node);
            for (size_t k = 0; k < ids.size(); ++k)
                if (k + 1 < ids.size()) fire(node, ids[k]);
                else tailFire(node, ids[k]);
        }
        else if (op == "flow/branch") tailFire(node, input(node, "condition").b ? "true" : "false");
        else if (op == "flow/switch")
        {
            const int32_t sel = input(node, "selection").i;
            std::vector<int32_t> cases;
            if (!configInts(node, "cases", cases)) cases.clear();
            if (std::find(cases.begin(), cases.end(), sel) != cases.end()) tailFire(node, std::to_string(sel));
            else tailFire(node, "default");
        }
        else if (op == "flow/while")
        {
            while (!aborted && input(node, "condition").b)
            {
                fire(node, "loopBody");
                if (++steps > kStepBudget) abort("flow/while ran more than the step budget");
            }
            if (!aborted) tailFire(node, "completed");
        }
        else if (op == "flow/for")
        {
            int32_t index = input(node, "startIndex").i;
            o["index"] = Value::ofInt(index);
            while (!aborted && index < input(node, "endIndex").i)
            {
                fire(node, "loopBody");
                index = wrap(static_cast<int64_t>(index) + 1);
                o["index"] = Value::ofInt(index);
                if (++steps > kStepBudget) abort("flow/for ran more than the step budget");
            }
            if (!aborted) tailFire(node, "completed");
        }
        else if (op == "flow/doN")
        {
            if (socket == "reset") { o["currentCount"] = Value::ofInt(0); return; }
            const int32_t n = input(node, "n").i;
            const int32_t count = o["currentCount"].i;
            if (count < n)
            {
                o["currentCount"] = Value::ofInt(count + 1);
                tailFire(node, "out");
            }
        }
        else if (op == "flow/multiGate")
        {
            const auto outputs = flowOutputs(node);
            if (socket == "reset")
            {
                o["lastIndex"] = Value::ofInt(-1);
                s.used.assign(outputs.size(), false);
                return;
            }
            bool isRandom = false, isLoop = false;
            if (!configBool(node, "isRandom", isRandom) || !configBool(node, "isLoop", isLoop)) isRandom = isLoop = false;
            s.used.resize(outputs.size(), false);
            auto pick = [&]() -> int {
                std::vector<int> free;
                for (size_t k = 0; k < s.used.size(); ++k)
                    if (!s.used[k]) free.push_back(static_cast<int>(k));
                if (free.empty()) return -1;
                if (!isRandom) return free.front();
                return free[std::min(free.size() - 1, static_cast<size_t>(host.random() * free.size()))];
            };
            int i = pick();
            if (i < 0 && isLoop && !outputs.empty())
            {
                s.used.assign(outputs.size(), false);
                i = pick();
            }
            if (i >= 0)
            {
                s.used[static_cast<size_t>(i)] = true;
                o["lastIndex"] = Value::ofInt(i);
                tailFire(node, outputs[static_cast<size_t>(i)]);
            }
        }
        else if (op == "flow/waitAll")
        {
            int32_t inputs = 0;
            if (!configInt(node, "inputFlows", inputs) || inputs < 0 || inputs > 64) inputs = 0;
            if (socket == "reset")
            {
                o["remainingInputs"] = Value::ofInt(inputs);
                s.used.assign(static_cast<size_t>(inputs), false);
                return;
            }
            char *end = nullptr;
            const long k = std::strtol(socket.c_str(), &end, 10);
            if (end == socket.c_str() || *end || k < 0 || k >= inputs) return;
            s.used.resize(static_cast<size_t>(inputs), false);
            int32_t remaining = o["remainingInputs"].i;
            if (!s.used[static_cast<size_t>(k)])
            {
                s.used[static_cast<size_t>(k)] = true;
                o["remainingInputs"] = Value::ofInt(--remaining);
            }
            tailFire(node, remaining == 0 ? "completed" : "out");
        }
        else if (op == "flow/throttle")
        {
            if (socket == "reset") { o["lastRemainingTime"] = Value::ofFloat(kNaN); return; }
            const double duration = input(node, "duration").f[0];
            if (!std::isfinite(duration) || duration < 0) { tailFire(node, "err"); return; }
            const double last = o["lastRemainingTime"].f[0];
            if (!std::isnan(last))
            {
                const double elapsed = std::max(0.0, time - s.timestamp);
                if (duration <= elapsed)
                {
                    s.timestamp = time;
                    o["lastRemainingTime"] = Value::ofFloat(0);
                    tailFire(node, "out");
                }
                else o["lastRemainingTime"] = Value::ofFloat(duration - elapsed);
                return;
            }
            s.timestamp = time;
            o["lastRemainingTime"] = Value::ofFloat(0);
            tailFire(node, "out");
        }
        else if (op == "flow/setDelay")
        {
            if (socket == "cancel")
            {
                o["lastDelay"] = Value::defaultOf(Sig::Ref);
                for (const uint64_t id : s.delays)
                    delays.erase(std::remove_if(delays.begin(), delays.end(), [id](const Delay &d) { return d.id == id; }),
                                 delays.end());
                s.delays.clear();
                return;
            }
            const double duration = input(node, "duration").f[0];
            if (!std::isfinite(duration) || duration < 0 || duration > kMaxDuration || delays.size() >= kMaxActive)
            {
                tailFire(node, "err");
                return;
            }
            const uint64_t id = nextId++;
            delays.push_back(Delay{id, time + duration, node});
            s.delays.push_back(id);
            o["lastDelay"] = Value::ofRef(Ref{Ref::Kind::Delay, {}, -1, id});
            tailFire(node, "out");
        }
        else if (op == "flow/cancelDelay")
        {
            const Value d = input(node, "delay");
            if (d.ref.kind == Ref::Kind::Delay) cancelDelay(d.ref.id);
            tailFire(node, "out");
        }
        else if (op == "variable/set")
        {
            std::vector<int32_t> vars;
            configInts(node, "variables", vars);
            std::vector<std::pair<int32_t, Value>> values;
            std::set<int32_t> seen;
            for (const int32_t v : vars)
                if (v >= 0 && static_cast<size_t>(v) < variables.size() && seen.insert(v).second)
                    values.emplace_back(v, input(node, std::to_string(v)));
            for (auto &[v, value] : values)
            {
                variables[static_cast<size_t>(v)] = value;
                const int32_t var = v;
                interpolations.erase(std::remove_if(interpolations.begin(), interpolations.end(),
                                                    [var](const Interpolation &e) { return e.variable == var; }),
                                     interpolations.end());
            }
            tailFire(node, "out");
        }
        else if (op == "variable/interpolate")
        {
            int32_t var = -1;
            bool slerp = false;
            configInt(node, "variable", var);
            configBool(node, "useSlerp", slerp);
            Interpolation e;
            if (var < 0 || static_cast<size_t>(var) >= variables.size() || !validEasing(node, e.duration, e.p1, e.p2) ||
                std::count_if(interpolations.begin(), interpolations.end(),
                              [](const Interpolation &x) { return x.variable >= 0; }) >= static_cast<long>(kMaxActive))
            {
                tailFire(node, "err");
                return;
            }
            interpolations.erase(std::remove_if(interpolations.begin(), interpolations.end(),
                                                [var](const Interpolation &x) { return x.variable == var; }),
                                 interpolations.end());
            e.id = nextId++;
            e.variable = var;
            e.start = time;
            e.from = variables[static_cast<size_t>(var)];
            e.to = input(node, "value");
            e.to.type = e.from.type;
            e.slerp = slerp && e.from.type == Sig::Float4;
            e.node = node;
            interpolations.push_back(e);
            tailFire(node, "out");
        }
        else if (op == "pointer/set")
        {
            int32_t t = -1;
            configInt(node, "type", t);
            Value value = input(node, "value");
            value.type = typeAt(t);
            std::string pointer;
            const bool resolved = effectivePointer(node, pointer) && pointer.rfind("/extensions/KHR_interactivity/", 0) != 0;
            if (resolved)
                interpolations.erase(std::remove_if(interpolations.begin(), interpolations.end(),
                                                    [&](const Interpolation &e) { return e.variable < 0 && e.pointer == pointer; }),
                                     interpolations.end());
            tailFire(node, resolved && host.setPointer(pointer, value) ? "out" : "err");
        }
        else if (op == "pointer/interpolate")
        {
            int32_t t = -1;
            configInt(node, "type", t);
            const Sig sig = typeAt(t);
            Interpolation e;
            e.from = Value::defaultOf(sig);
            // Read the property, and write it back unchanged to learn that it's
            // mutable (the host has no other way to tell).
            if (!isFloatType(sig) || !effectivePointer(node, e.pointer) ||
                e.pointer.rfind("/extensions/KHR_interactivity/", 0) == 0 || !validEasing(node, e.duration, e.p1, e.p2) ||
                std::count_if(interpolations.begin(), interpolations.end(),
                              [](const Interpolation &x) { return x.variable < 0; }) >= static_cast<long>(kMaxActive) ||
                !getPointer(e.pointer, sig, e.from) || !host.setPointer(e.pointer, e.from))
            {
                tailFire(node, "err");
                return;
            }
            const std::string pointer = e.pointer;
            interpolations.erase(std::remove_if(interpolations.begin(), interpolations.end(),
                                                [&](const Interpolation &x) { return x.variable < 0 && x.pointer == pointer; }),
                                 interpolations.end());
            e.id = nextId++;
            e.start = time;
            e.to = input(node, "value");
            e.to.type = sig;
            e.from.type = sig;
            // Quaternion properties interpolate spherically.
            e.slerp = sig == Sig::Float4 && pointer.size() >= 9 && pointer.compare(pointer.size() - 9, 9, "/rotation") == 0;
            e.node = node;
            interpolations.push_back(e);
            tailFire(node, "out");
        }
        else if (op == "animation/start")
        {
            const int64_t a = animationIndex(input(node, "animation"));
            const double startTime = input(node, "startTime").f[0], endTime = input(node, "endTime").f[0];
            const double speed = input(node, "speed").f[0];
            if (a < 0 || std::isnan(startTime) || !std::isfinite(startTime) || std::isnan(endTime) ||
                !std::isfinite(speed) || speed <= 0 || !animations[static_cast<size_t>(a)].valid ||
                playbacks.size() >= kMaxActive)
            {
                tailFire(node, "err");
                return;
            }
            // A new start replaces the one playing; its `done` flows don't run.
            playbacks.erase(std::remove_if(playbacks.begin(), playbacks.end(), [a](const Playback &p) { return p.animation == a; }),
                            playbacks.end());
            playbacks.push_back(Playback{nextId++, a, startTime, endTime, endTime, speed, time, node, -1});
            tailFire(node, "out");
        }
        else if (op == "animation/stop")
        {
            const int64_t a = animationIndex(input(node, "animation"));
            if (a < 0) { tailFire(node, "err"); return; }
            const auto playing = std::remove_if(playbacks.begin(), playbacks.end(), [a](const Playback &p) { return p.animation == a; });
            const bool was = playing != playbacks.end();
            playbacks.erase(playing, playbacks.end());
            if (was) host.animationStopped(a);
            tailFire(node, "out");
        }
        else if (op == "animation/stopAt")
        {
            const int64_t a = animationIndex(input(node, "animation"));
            const double stopTime = input(node, "stopTime").f[0];
            if (a < 0 || std::isnan(stopTime)) { tailFire(node, "err"); return; }
            for (auto &p : playbacks)
                if (p.animation == a)
                {
                    p.stopTime = stopTime;
                    p.stopNode = node;
                }
            tailFire(node, "out");
        }
        else if (op == "debug/log")
        {
            std::string message = configString(node, "message"), text;
            for (size_t i = 0; i < message.size(); ++i)
            {
                if (message[i] == '{' && i + 1 < message.size() && message[i + 1] == '{') { text += '{'; ++i; }
                else if (message[i] == '}' && i + 1 < message.size() && message[i + 1] == '}') { text += '}'; ++i; }
                else if (message[i] == '{')
                {
                    const size_t close = message.find('}', i);
                    if (close == std::string::npos) { text += message.substr(i); break; }
                    text += formatValue(input(node, message.substr(i + 1, close - i - 1)));
                    i = close;
                }
                else text += message[i];
            }
            host.log(text);
            tailFire(node, "out");
        }
        else if (op == "event/stopPropagation")
        {
            const Value e = input(node, "event");
            if (e.ref.kind == Ref::Kind::Event && e.ref == currentEvent)
            {
                stopTransitive = true;
                if (input(node, "stopImmediate").b) stopImmediate = true;
            }
            tailFire(node, "out");
        }
        else if (op == "event/send")
        {
            int32_t e = -1;
            if (configInt(node, "event", e) && e >= 0 && static_cast<size_t>(e) < graph->events.size())
            {
                std::map<std::string, Value> values;
                for (const auto &[id, ev] : graph->events[static_cast<size_t>(e)].values) values[id] = input(node, id);
                host.eventSent(e, values);
                queued.push_back(Received{e, values});
            }
            tailFire(node, "out");
        }
        // Event nodes (onStart, onTick, onSelect, receive) are activated by the
        // runtime itself (dispatch()), which sets their outputs first.
        else if (op.rfind("event/on", 0) == 0 || op == "event/receive") tailFire(node, "out");
    }

    void cancelDelay(uint64_t id)
    {
        delays.erase(std::remove_if(delays.begin(), delays.end(), [id](const Delay &d) { return d.id == id; }), delays.end());
        for (auto &s : state) s.delays.erase(std::remove(s.delays.begin(), s.delays.end(), id), s.delays.end());
    }

    // Runs one event: `handlers` in order, stopping when a handler stops
    // immediate propagation. Returns whether any handler ran.
    bool dispatch(const std::vector<int64_t> &handlers, const Ref &event,
                  const std::function<void(int64_t)> &setOutputs)
    {
        if (handlers.empty()) return false;
        steps = 0;
        aborted = false;
        depth = 0;
        currentEvent = event;
        stopImmediate = false;
        for (const int64_t h : handlers)
        {
            if (stopImmediate) break;
            setOutputs(h);
            state[static_cast<size_t>(h)].outputs["event"] = Value::ofRef(event);
            activate(h, "in");
        }
        return true;
    }

    std::vector<int64_t> nodesWithOp(const std::string &op) const
    {
        std::vector<int64_t> list;
        for (size_t i = 0; i < ops.size(); ++i)
            if (ops[i] == op) list.push_back(static_cast<int64_t>(i));
        return list;
    }

    Ref newEvent() { return Ref{Ref::Kind::Event, {}, -1, nextId++}; }
};

// ---- public API --------------------------------------------------------------------

InteractivityRuntime::InteractivityRuntime(const GLTF &asset, InteractivityHost &host)
    : impl_(std::make_unique<Impl>(asset, host))
{
}

InteractivityRuntime::~InteractivityRuntime() = default;

bool InteractivityRuntime::valid() const { return impl_->graph != nullptr; }

void InteractivityRuntime::start()
{
    auto &d = *impl_;
    if (!d.graph || d.started) return;
    d.started = true;
    d.time = 0;
    d.dispatch(d.nodesWithOp("event/onStart"), d.newEvent(), [](int64_t) {});
}

void InteractivityRuntime::tick(double seconds)
{
    auto &d = *impl_;
    if (!d.graph) return;
    if (!d.started) start();
    if (d.ticked && std::isfinite(seconds) && seconds > 0) d.time += seconds;

    d.updateAnimations();
    d.updateInterpolations();

    // Delays that are due, in order of their activation time.
    for (;;)
    {
        auto due = std::min_element(d.delays.begin(), d.delays.end(), [](const auto &a, const auto &b) { return a.at < b.at; });
        if (due == d.delays.end() || due->at > d.time) break;
        const auto delay = *due;
        d.cancelDelay(delay.id);
        d.steps = 0;
        d.aborted = false;
        d.depth = 0;
        d.fire(delay.node, "done");  // a nested activation from depth 0
    }

    // Custom events the graph sent itself.
    auto queued = std::move(d.queued);
    d.queued.clear();
    for (const auto &e : queued) receive(e.event, e.values);

    const double sinceLast = d.ticked ? d.time - d.lastTick : std::numeric_limits<double>::quiet_NaN();
    const double sinceStart = d.time;
    d.ticked = true;
    d.lastTick = d.time;
    d.dispatch(d.nodesWithOp("event/onTick"), d.newEvent(), [&](int64_t h) {
        auto &o = d.state[static_cast<size_t>(h)].outputs;
        o["timeSinceStart"] = Value::ofFloat(sinceStart);
        o["timeSinceLastTick"] = Value::ofFloat(sinceLast);
    });
}

bool InteractivityRuntime::select(int64_t node, int32_t controllerIndex, const double *selectionPoint,
                                  const double *selectionRayOrigin)
{
    auto &d = *impl_;
    if (!d.graph || node < 0) return false;
    if (!d.started) start();
    // The node, then its ancestors (the propagation rule), each with its
    // handlers in JSON order.
    std::vector<int64_t> chain;
    for (int64_t n = node; n >= 0; n = static_cast<size_t>(n) < d.parent.size() ? d.parent[static_cast<size_t>(n)] : -1)
        chain.push_back(n);
    const auto handlers = d.nodesWithOp("event/onSelect");
    const Ref event = d.newEvent();
    double nan3[3] = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::quiet_NaN()};
    bool any = false;
    d.stopTransitive = false;
    for (const int64_t target : chain)
    {
        if (d.stopTransitive) break;
        std::vector<int64_t> here;
        for (const int64_t h : handlers)
        {
            int32_t n = -1;
            if (d.configInt(h, "nodeIndex", n) && n == target) here.push_back(h);
        }
        any = d.dispatch(here, event, [&](int64_t h) {
                  auto &o = d.state[static_cast<size_t>(h)].outputs;
                  o["selectedNode"] = Value::ofRef(Ref{Ref::Kind::Object, "nodes", node, 0});
                  o["controllerIndex"] = Value::ofInt(controllerIndex);
                  o["selectionPoint"] = Value::ofFloats(Sig::Float3, selectionPoint ? selectionPoint : nan3);
                  o["selectionRayOrigin"] = Value::ofFloats(Sig::Float3, selectionRayOrigin ? selectionRayOrigin : nan3);
              }) || any;
    }
    return any;
}

bool InteractivityRuntime::hover(int64_t node, int32_t controllerIndex)
{
    auto &d = *impl_;
    if (!d.graph) return false;
    if (!d.started) start();
    if (node < 0) node = -1;
    const auto previous = d.hovered.find(controllerIndex);
    const int64_t before = previous == d.hovered.end() ? -1 : previous->second;
    if (before == node) return false;
    d.hovered[controllerIndex] = node;

    auto chainOf = [&](int64_t n) {
        std::vector<int64_t> chain;
        for (; n >= 0; n = static_cast<size_t>(n) < d.parent.size() ? d.parent[static_cast<size_t>(n)] : -1)
            chain.push_back(n);
        return chain;
    };
    const auto left = chainOf(before), entered = chainOf(node);
    const std::set<int64_t> inLeft(left.begin(), left.end()), inEntered(entered.begin(), entered.end());

    // From the hovered node up, stopping at the subtree that holds both the
    // previous and the current target (the lowest common ancestor).
    auto run = [&](const char *op, const std::vector<int64_t> &chain, const std::set<int64_t> &other, int64_t target) {
        const auto handlers = d.nodesWithOp(op);
        const Ref event = d.newEvent();
        bool any = false;
        d.stopTransitive = false;
        for (const int64_t n : chain)
        {
            if (other.count(n) || d.stopTransitive) break;
            std::vector<int64_t> here;
            for (const int64_t h : handlers)
            {
                int32_t index = -1;
                if (d.configInt(h, "nodeIndex", index) && index == n) here.push_back(h);
            }
            any = d.dispatch(here, event, [&](int64_t h) {
                      auto &o = d.state[static_cast<size_t>(h)].outputs;
                      o["hoveredNode"] = Value::ofRef(Ref{Ref::Kind::Object, "nodes", target, 0});
                      o["controllerIndex"] = Value::ofInt(controllerIndex);
                  }) || any;
        }
        return any;
    };
    bool any = false;
    if (before >= 0) any = run("event/onHoverOut", left, inEntered, before) || any;
    if (node >= 0) any = run("event/onHoverIn", entered, inLeft, node) || any;
    return any;
}

bool InteractivityRuntime::usesHover() const
{
    const auto &d = *impl_;
    return std::any_of(d.ops.begin(), d.ops.end(),
                       [](const std::string &op) { return op == "event/onHoverIn" || op == "event/onHoverOut"; });
}

bool InteractivityRuntime::animating() const
{
    return !impl_->playbacks.empty() || !impl_->interpolations.empty();
}

bool InteractivityRuntime::selectable(int64_t node) const
{
    const auto &d = *impl_;
    if (!d.graph) return false;
    std::set<int32_t> targets;
    for (size_t i = 0; i < d.ops.size(); ++i)
    {
        int32_t n = -1;
        if (d.ops[i] == "event/onSelect" && d.configInt(static_cast<int64_t>(i), "nodeIndex", n)) targets.insert(n);
    }
    for (int64_t n = node; n >= 0;
         n = static_cast<size_t>(n) < d.parent.size() ? d.parent[static_cast<size_t>(n)] : -1)
        if (targets.count(static_cast<int32_t>(n))) return true;
    return false;
}

void InteractivityRuntime::receive(int64_t eventIndex, const std::map<std::string, InteractivityRuntimeValue> &values)
{
    auto &d = *impl_;
    if (!d.graph) return;
    if (!d.started) start();
    std::vector<int64_t> handlers;
    for (const int64_t h : d.nodesWithOp("event/receive"))
    {
        int32_t e = -1;
        if (d.configInt(h, "event", e) && e == eventIndex) handlers.push_back(h);
    }
    d.dispatch(handlers, d.newEvent(), [&](int64_t h) {
        auto &o = d.state[static_cast<size_t>(h)].outputs;
        for (const auto &[id, v] : values)
            if (o.count(id)) o[id] = v;
    });
}

const std::vector<std::string> &InteractivityRuntime::unsupportedOperations() const { return impl_->unsupported; }

const std::vector<std::string> &InteractivityRuntime::errors() const { return impl_->errors; }

} // namespace systems::leal::gltf
