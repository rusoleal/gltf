// KHR_interactivity parsing and validation.
//
// Follows the "Validation" section of the specification
// (https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_interactivity/Specification.adoc):
//  - an "assert" failure rejects the whole extension object;
//  - a "reject the graph" failure invalidates only that graph.
// Neither fails the glTF load.

#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

#include "utils.hpp"
#include "khr_interactivity_ops.hpp"

namespace
{
    // Thrown when a structural assert fails: the whole extension is rejected.
    struct ExtensionRejected
    {
        std::string message;
    };

    constexpr double kMaxIndex = 2147483648.0; // JSON index upper bound (exclusive)

    const nlohmann::json &ixMember(const nlohmann::json &obj, const char *key)
    {
        static const nlohmann::json null;
        if (!obj.is_object())
            return null;
        const auto it = obj.find(key);
        return it == obj.end() ? null : *it;
    }

    bool ixHas(const nlohmann::json &obj, const char *key)
    {
        return obj.is_object() && obj.find(key) != obj.end();
    }

    // A "JSON index": a non-negative number below 2^31 exactly representable as an integer (2, 2.0 and 0.2e1 all qualify).
    bool isJsonIndex(const nlohmann::json &v)
    {
        if (v.is_number_unsigned())
            return v.get<uint64_t>() < static_cast<uint64_t>(kMaxIndex);
        if (v.is_number_integer())
            return v.get<int64_t>() >= 0 && v.get<int64_t>() < static_cast<int64_t>(kMaxIndex);
        if (v.is_number_float())
        {
            const double d = v.get<double>();
            return d >= 0 && d < kMaxIndex && std::floor(d) == d;
        }
        return false;
    }

    // A JSON number exactly representable as a 32-bit signed integer.
    bool isInt32(const nlohmann::json &v, int64_t &out)
    {
        if (v.is_number_unsigned())
        {
            const uint64_t u = v.get<uint64_t>();
            if (u > 2147483647u)
                return false;
            out = static_cast<int64_t>(u);
            return true;
        }
        if (v.is_number_integer())
        {
            out = v.get<int64_t>();
            return out >= -2147483648LL && out <= 2147483647LL;
        }
        if (v.is_number_float())
        {
            const double d = v.get<double>();
            if (!(d >= -2147483648.0 && d <= 2147483647.0) || std::floor(d) != d)
                return false;
            out = static_cast<int64_t>(d);
            return true;
        }
        return false;
    }

    // RFC 6901 syntax: empty, or '/'-prefixed with '~' only in "~0" / "~1".
    bool isJsonPointer(const std::string &s)
    {
        if (s.empty())
            return true;
        if (s[0] != '/')
            return false;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '~' && (i + 1 >= s.size() || (s[i + 1] != '0' && s[i + 1] != '1')))
                return false;
        }
        return true;
    }

    // Number of array elements a value of this type has; 0 for custom (any length).
    size_t valueLength(InteractivityTypeSignature sig)
    {
        switch (sig)
        {
        case InteractivityTypeSignature::Bool:
        case InteractivityTypeSignature::Int:
        case InteractivityTypeSignature::Float:
        case InteractivityTypeSignature::Ref:
            return 1;
        case InteractivityTypeSignature::Float2:
            return 2;
        case InteractivityTypeSignature::Float3:
            return 3;
        case InteractivityTypeSignature::Float4:
        case InteractivityTypeSignature::Float2x2:
            return 4;
        case InteractivityTypeSignature::Float3x3:
            return 9;
        case InteractivityTypeSignature::Float4x4:
            return 16;
        case InteractivityTypeSignature::Custom:
            return 0;
        }
        return 0;
    }

    bool signatureFromString(const std::string &s, InteractivityTypeSignature &out)
    {
        static const std::pair<const char *, InteractivityTypeSignature> kSignatures[] = {
            {"bool", InteractivityTypeSignature::Bool},
            {"int", InteractivityTypeSignature::Int},
            {"float", InteractivityTypeSignature::Float},
            {"float2", InteractivityTypeSignature::Float2},
            {"float3", InteractivityTypeSignature::Float3},
            {"float4", InteractivityTypeSignature::Float4},
            {"float2x2", InteractivityTypeSignature::Float2x2},
            {"float3x3", InteractivityTypeSignature::Float3x3},
            {"float4x4", InteractivityTypeSignature::Float4x4},
            {"ref", InteractivityTypeSignature::Ref},
            {"custom", InteractivityTypeSignature::Custom},
        };
        for (const auto &entry : kSignatures)
        {
            if (s == entry.first)
            {
                out = entry.second;
                return true;
            }
        }
        return false;
    }

    class GraphParser
    {
    public:
        explicit GraphParser(std::string context) : context_(std::move(context)) {}

        InteractivityGraph parse(const nlohmann::json &data)
        {
            assertThat(data.is_object(), context_, "must be an object");

            if (ixHas(data, "name"))
            {
                assertThat(ixMember(data, "name").is_string(), context_, "'name' must be a string");
                graph_.name = ixMember(data, "name").get<std::string>();
            }

            parseTypes(data);
            parseVariables(data);
            parseEvents(data);
            parseDeclarations(data);
            parseNodes(data);

            return std::move(graph_);
        }

    private:
        std::string context_;
        InteractivityGraph graph_;
        bool hasTypes_ = false;
        bool hasDeclarations_ = false;

        // ---- error handling ------------------------------------------------

        [[noreturn]] static void rejectExtension(const std::string &ctx, const std::string &message)
        {
            throw ExtensionRejected{ctx + " " + message};
        }

        static void assertThat(bool condition, const std::string &ctx, const std::string &message)
        {
            if (!condition)
                rejectExtension(ctx, message);
        }

        // Keeps the first reason; parsing continues so later structural asserts still apply.
        void rejectGraph(const std::string &ctx, const std::string &message)
        {
            if (graph_.valid)
            {
                graph_.valid = false;
                graph_.error = ctx + " " + message;
            }
        }

        // ---- shared helpers ------------------------------------------------

        // Asserts the member, if present, is a non-empty array of objects; calls fn for each.
        template <typename F>
        void forEachItem(const nlohmann::json &obj, const char *key, F &&fn)
        {
            if (!ixHas(obj, key))
                return;
            const auto &arr = ixMember(obj, key);
            const std::string ctx = context_ + "." + key;
            assertThat(arr.is_array() && !arr.empty(), ctx, "must be a non-empty array");
            assertThat(static_cast<double>(arr.size()) <= kMaxIndex, ctx, "has too many elements");
            for (size_t i = 0; i < arr.size(); ++i)
            {
                const std::string itemCtx = ctx + "[" + std::to_string(i) + "]";
                assertThat(arr[i].is_object(), itemCtx, "must be an object");
                fn(arr[i], itemCtx, static_cast<int64_t>(i));
            }
        }

        // Asserts the member, if present, is a non-empty object whose values are objects; calls fn for each.
        template <typename F>
        static void forEachEntry(const nlohmann::json &obj, const char *key, const std::string &ctx, F &&fn)
        {
            if (!ixHas(obj, key))
                return;
            const auto &map = ixMember(obj, key);
            const std::string mapCtx = ctx + "." + key;
            assertThat(map.is_object() && !map.empty(), mapCtx, "must be a non-empty object");
            for (const auto &entry : map.items())
            {
                const std::string entryCtx = mapCtx + "['" + entry.key() + "']";
                assertThat(entry.value().is_object(), entryCtx, "must be an object");
                fn(entry.key(), entry.value(), entryCtx);
            }
        }

        static void readOptionalString(const nlohmann::json &obj, const char *key, const std::string &ctx, std::string &out)
        {
            if (!ixHas(obj, key))
                return;
            assertThat(ixMember(obj, key).is_string(), ctx, std::string("'") + key + "' must be a string");
            out = ixMember(obj, key).get<std::string>();
        }

        // A required JSON index (assert), additionally range-checked against `size` (reject the graph).
        int64_t readIndex(const nlohmann::json &obj, const char *key, const std::string &ctx, size_t size, const char *what)
        {
            const auto &v = ixMember(obj, key);
            assertThat(ixHas(obj, key), ctx, std::string("is missing required '") + key + "'");
            assertThat(isJsonIndex(v), ctx, std::string("'") + key + "' must be a non-negative integer index");
            const int64_t index = static_cast<int64_t>(v.get<double>());
            if (static_cast<uint64_t>(index) >= size)
                rejectGraph(ctx, std::string("references out-of-range ") + what + " index " + std::to_string(index));
            return index;
        }

        int64_t readTypeIndex(const nlohmann::json &obj, const std::string &ctx)
        {
            return readIndex(obj, "type", ctx, graph_.types.size(), "type");
        }

        const InteractivityType *typeAt(int64_t index) const
        {
            if (index < 0 || static_cast<uint64_t>(index) >= graph_.types.size())
                return nullptr;
            return &graph_.types[static_cast<size_t>(index)];
        }

        // Converts a JSON literal array (asserted non-empty) and validates it against the type
        // ("Inline Value Object Validation"; a failure rejects the graph).
        InteractivityValue readValue(const nlohmann::json &data, int64_t typeIndex, const std::string &ctx)
        {
            assertThat(data.is_array() && !data.empty(), ctx, "'value' must be a non-empty array");

            const InteractivityType *type = typeAt(typeIndex);
            const InteractivityTypeSignature sig = type ? type->signature : InteractivityTypeSignature::Custom;

            InteractivityValue value;
            value.reserve(data.size());
            for (const auto &item : data)
            {
                InteractivityLiteral lit;
                if (item.is_boolean())
                {
                    lit.kind = InteractivityLiteral::Kind::Bool;
                    lit.boolValue = item.get<bool>();
                    lit.numberValue = lit.boolValue ? 1 : 0;
                }
                else if (item.is_number())
                {
                    int64_t i = 0;
                    if (item.is_number_integer() || (sig == InteractivityTypeSignature::Int && isInt32(item, i)))
                    {
                        lit.kind = InteractivityLiteral::Kind::Integer;
                        lit.intValue = item.is_number_unsigned() ? static_cast<int64_t>(item.get<uint64_t>())
                                     : item.is_number_integer()  ? item.get<int64_t>()
                                                                 : i;
                        lit.numberValue = static_cast<GLTF_REAL_NUMBER_TYPE>(item.get<double>());
                    }
                    else
                    {
                        lit.kind = InteractivityLiteral::Kind::Number;
                        lit.numberValue = item.get<GLTF_REAL_NUMBER_TYPE>();
                    }
                }
                else if (item.is_string())
                {
                    lit.kind = InteractivityLiteral::Kind::String;
                    lit.stringValue = item.get<std::string>();
                }
                value.push_back(std::move(lit));
            }

            if (!type || sig == InteractivityTypeSignature::Custom)
                return value; // out-of-range type already rejected; custom values are extension-defined

            const std::string &sigName = type->signatureName;
            if (data.size() != valueLength(sig))
            {
                rejectGraph(ctx, "value of type " + sigName + " must have exactly " + std::to_string(valueLength(sig)) + " element(s)");
                return value;
            }
            for (const auto &item : data)
            {
                int64_t i = 0;
                switch (sig)
                {
                case InteractivityTypeSignature::Bool:
                    if (!item.is_boolean())
                        rejectGraph(ctx, "bool value must be a JSON boolean");
                    break;
                case InteractivityTypeSignature::Int:
                    if (!isInt32(item, i))
                        rejectGraph(ctx, "int value must be a 32-bit signed integer");
                    break;
                case InteractivityTypeSignature::Ref:
                    if (!item.is_string() || !isJsonPointer(item.get<std::string>()))
                        rejectGraph(ctx, "ref value must be a JSON Pointer string");
                    break;
                default: // float types
                    if (!item.is_number())
                        rejectGraph(ctx, "value of type " + sigName + " must contain only numbers");
                    break;
                }
            }
            return value;
        }

        InteractivityValue readOptionalValue(const nlohmann::json &obj, int64_t typeIndex, const std::string &ctx)
        {
            if (!ixHas(obj, "value"))
                return {};
            return readValue(ixMember(obj, "value"), typeIndex, ctx);
        }

        // ---- sections ------------------------------------------------------

        void parseTypes(const nlohmann::json &data)
        {
            hasTypes_ = ixHas(data, "types");
            std::set<std::string> seen;
            forEachItem(data, "types", [&](const nlohmann::json &def, const std::string &ctx, int64_t) {
                assertThat(ixMember(def, "signature").is_string(), ctx, "'signature' must be a string");
                InteractivityType type;
                type.signatureName = ixMember(def, "signature").get<std::string>();
                if (!signatureFromString(type.signatureName, type.signature))
                    rejectGraph(ctx, "has unknown type signature '" + type.signatureName + "'");
                else if (type.signature != InteractivityTypeSignature::Custom && !seen.insert(type.signatureName).second)
                    rejectGraph(ctx, "duplicates type signature '" + type.signatureName + "'");
                graph_.types.push_back(std::move(type));
            });
        }

        void parseVariables(const nlohmann::json &data)
        {
            if (ixHas(data, "variables"))
                assertThat(hasTypes_, context_, "defines 'variables' without 'types'");
            forEachItem(data, "variables", [&](const nlohmann::json &def, const std::string &ctx, int64_t) {
                InteractivityVariable variable;
                variable.type = readTypeIndex(def, ctx);
                variable.value = readOptionalValue(def, variable.type, ctx);
                readOptionalString(def, "name", ctx, variable.name);
                graph_.variables.push_back(std::move(variable));
            });
        }

        void parseEvents(const nlohmann::json &data)
        {
            std::set<std::string> ids;
            forEachItem(data, "events", [&](const nlohmann::json &def, const std::string &ctx, int64_t) {
                InteractivityEvent event;
                if (ixHas(def, "id"))
                {
                    readOptionalString(def, "id", ctx, event.id);
                    if (!ids.insert(event.id).second)
                        rejectGraph(ctx, "duplicates event id '" + event.id + "'");
                }
                forEachEntry(def, "values", ctx, [&](const std::string &key, const nlohmann::json &vdef, const std::string &vctx) {
                    assertThat(key != "event", vctx, "uses the reserved socket id 'event'");
                    InteractivityEventValue ev;
                    ev.type = readTypeIndex(vdef, vctx);
                    ev.value = readOptionalValue(vdef, ev.type, vctx);
                    event.values.emplace(key, std::move(ev));
                });
                readOptionalString(def, "name", ctx, event.name);
                graph_.events.push_back(std::move(event));
            });
        }

        void parseDeclarations(const nlohmann::json &data)
        {
            hasDeclarations_ = ixHas(data, "declarations");
            forEachItem(data, "declarations", [&](const nlohmann::json &def, const std::string &ctx, int64_t) {
                InteractivityDeclaration decl;
                assertThat(ixMember(def, "op").is_string(), ctx, "'op' must be a string");
                decl.op = ixMember(def, "op").get<std::string>();

                const bool hasExtension = ixHas(def, "extension");
                readOptionalString(def, "extension", ctx, decl.extension);

                auto readSockets = [&](const char *key, std::map<std::string, InteractivityDeclarationSocket> &out) {
                    if (ixHas(def, key))
                        assertThat(hasTypes_, ctx, std::string("defines '") + key + "' without graph 'types'");
                    forEachEntry(def, key, ctx, [&](const std::string &name, const nlohmann::json &sdef, const std::string &sctx) {
                        InteractivityDeclarationSocket socket;
                        socket.type = readTypeIndex(sdef, sctx);
                        out.emplace(name, socket);
                    });
                };
                if (hasExtension)
                {
                    readSockets("inputValueSockets", decl.inputValueSockets);
                    readSockets("outputValueSockets", decl.outputValueSockets);
                }
                else if (ixHas(def, "inputValueSockets") || ixHas(def, "outputValueSockets"))
                {
                    // Operations of this Specification define their own sockets.
                    rejectGraph(ctx, "declares value sockets for an operation without 'extension'");
                }

                // Equal declarations (op, extension, input socket ids and types) are all invalid.
                for (const auto &other : graph_.declarations)
                {
                    if (other.op == decl.op && other.extension == decl.extension &&
                        sameSockets(other.inputValueSockets, decl.inputValueSockets))
                    {
                        rejectGraph(ctx, "duplicates declaration '" + decl.op + "'");
                        break;
                    }
                }
                graph_.declarations.push_back(std::move(decl));
            });
        }

        static bool sameSockets(const std::map<std::string, InteractivityDeclarationSocket> &a,
                                const std::map<std::string, InteractivityDeclarationSocket> &b)
        {
            if (a.size() != b.size())
                return false;
            for (auto ia = a.begin(), ib = b.begin(); ia != a.end(); ++ia, ++ib)
            {
                if (ia->first != ib->first || ia->second.type != ib->second.type)
                    return false;
            }
            return true;
        }

        void parseNodes(const nlohmann::json &data)
        {
            if (ixHas(data, "nodes"))
                assertThat(hasDeclarations_, context_, "defines 'nodes' without 'declarations'");
            const size_t nodeCount = ixMember(data, "nodes").is_array() ? ixMember(data, "nodes").size() : 0;

            forEachItem(data, "nodes", [&](const nlohmann::json &def, const std::string &ctx, int64_t index) {
                InteractivityNode node;
                node.declaration = readIndex(def, "declaration", ctx, graph_.declarations.size(), "declaration");

                forEachEntry(def, "configuration", ctx, [&](const std::string &key, const nlohmann::json &cdef, const std::string &cctx) {
                    const auto &v = ixMember(cdef, "value");
                    assertThat(v.is_array() && !v.empty(), cctx, "'value' must be a non-empty array");
                    node.configuration.emplace(key, readValue(v, -1, cctx));
                });

                forEachEntry(def, "values", ctx, [&](const std::string &key, const nlohmann::json &vdef, const std::string &vctx) {
                    node.values.emplace(key, readNodeValue(vdef, vctx, index));
                });

                forEachEntry(def, "flows", ctx, [&](const std::string &key, const nlohmann::json &fdef, const std::string &fctx) {
                    InteractivityFlow flow;
                    assertThat(ixHas(fdef, "node") && isJsonIndex(ixMember(fdef, "node")), fctx, "'node' must be a non-negative integer index");
                    flow.node = static_cast<int64_t>(ixMember(fdef, "node").get<double>());
                    if (flow.node <= index || static_cast<uint64_t>(flow.node) >= nodeCount)
                        rejectGraph(fctx, "must point to a later node, not " + std::to_string(flow.node));
                    readOptionalString(fdef, "socket", fctx, flow.socket);
                    node.flows.emplace(key, std::move(flow));
                });

                graph_.nodes.push_back(std::move(node));
            });

            checkInteractivityOperations(graph_, context_);
        }

        InteractivityNodeValue readNodeValue(const nlohmann::json &vdef, const std::string &vctx, int64_t nodeIndex)
        {
            InteractivityNodeValue nv;
            if (ixHas(vdef, "node"))
            {
                assertThat(!ixHas(vdef, "value"), vctx, "must not have both 'node' and 'value'");
                assertThat(isJsonIndex(ixMember(vdef, "node")), vctx, "'node' must be a non-negative integer index");
                nv.node = static_cast<int64_t>(ixMember(vdef, "node").get<double>());
                if (nv.node >= nodeIndex)
                    rejectGraph(vctx, "must reference an earlier node, not " + std::to_string(nv.node));
                readOptionalString(vdef, "socket", vctx, nv.socket);
                if (ixHas(vdef, "type"))
                    nv.type = readTypeIndex(vdef, vctx);
            }
            else
            {
                nv.type = readTypeIndex(vdef, vctx);
                nv.value = readOptionalValue(vdef, nv.type, vctx);
            }
            return nv;
        }
    };
}

std::shared_ptr<KHRInteractivity> khrInteractivityFromGLTF(const nlohmann::json &data)
{
    auto result = std::make_shared<KHRInteractivity>();
    try
    {
        if (!data.is_object())
            throw ExtensionRejected{"extension must be an object"};

        const auto &graphsDef = ixMember(data, "graphs");
        if (!graphsDef.is_array() || graphsDef.empty())
            throw ExtensionRejected{"'graphs' must be a non-empty array"};
        if (static_cast<double>(graphsDef.size()) > kMaxIndex)
            throw ExtensionRejected{"'graphs' has too many elements"};

        result->graphs.reserve(graphsDef.size());
        for (size_t i = 0; i < graphsDef.size(); ++i)
            result->graphs.push_back(GraphParser("graphs[" + std::to_string(i) + "]").parse(graphsDef[i]));

        if (ixHas(data, "graph"))
        {
            if (!isJsonIndex(ixMember(data, "graph")))
                throw ExtensionRejected{"'graph' must be a non-negative integer index"};
            result->graph = static_cast<int64_t>(ixMember(data, "graph").get<double>());
        }
        if (static_cast<uint64_t>(result->graph) >= result->graphs.size())
            throw ExtensionRejected{"default graph index " + std::to_string(result->graph) + " is out of range"};

        const auto &selected = result->graphs[static_cast<size_t>(result->graph)];
        if (!selected.valid)
            throw ExtensionRejected{"default graph is invalid: " + selected.error};
    }
    catch (const ExtensionRejected &e)
    {
        result->valid = false;
        result->error = "KHR_interactivity: " + e.message;
        result->graphs.clear();
    }
    return result;
}
