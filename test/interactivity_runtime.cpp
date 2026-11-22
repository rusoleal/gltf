/**
 * KHR_interactivity runtime: small graphs written here, and the Calculator
 * test asset end to end (build/interactivity-assets, see
 * interactivity_conformance.cpp; skipped when absent).
 */
#include <gltf/extensions/khr_interactivity_runtime.hpp>
#include <gltf/gltf.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <map>
#include <string>
#include <vector>

using namespace systems::leal::gltf;
namespace fs = std::filesystem;

namespace
{
    using Sig = InteractivityTypeSignature;
    using Value = InteractivityRuntimeValue;

    // Records pointer/set, answers pointer/get from what it holds, logs.
    class FakeHost : public InteractivityHost
    {
    public:
        std::map<std::string, Value> properties;
        std::vector<std::string> sets;
        std::vector<std::string> logs;
        std::vector<int64_t> sent;
        double next = 0.25;

        bool getPointer(const std::string &pointer, Sig type, Value &value) override
        {
            const auto it = properties.find(pointer);
            if (it == properties.end() || it->second.type != type) return false;
            value = it->second;
            return true;
        }
        bool setPointer(const std::string &pointer, const Value &value) override
        {
            if (pointer.find("/nonexistent") != std::string::npos) return false;
            properties[pointer] = value;
            sets.push_back(pointer);
            return true;
        }
        void log(const std::string &message) override { logs.push_back(message); }
        void eventSent(int64_t eventIndex, const std::map<std::string, Value> &) override { sent.push_back(eventIndex); }
        double random() override { return next; }
        void applyAnimation(int64_t animation, double time) override { applied.emplace_back(animation, time); }
        void animationStopped(int64_t animation) override { stopped.push_back(animation); }
        std::vector<std::pair<int64_t, double>> applied;
        std::vector<int64_t> stopped;
    };


    auto kNoFiles = [](const std::string &) {
        return std::async(std::launch::deferred, [] { return std::vector<uint8_t>{}; });
    };

    // A glTF with `nodeCount` empty nodes (node 1 a child of node 0) and the
    // given KHR_interactivity graph (JSON).
    std::shared_ptr<GLTF> withGraph(const std::string &graph, int nodeCount = 3)
    {
        std::string nodes;
        for (int i = 0; i < nodeCount; ++i) nodes += std::string(i ? "," : "") + (i == 0 ? R"({"children": [1]})" : "{}");
        const std::string json = R"({"asset": {"version": "2.0"}, "extensionsUsed": ["KHR_interactivity"],
            "nodes": [)" + nodes + R"(], "extensions": {"KHR_interactivity": {"graph": 0, "graphs": [)" + graph + "]}}}";
        return GLTF::loadGLTF(json, kNoFiles);
    }

    // A glTF with one 2 s animation (node 0's translation; keyframes at 0 and
    // 2 s) and the given KHR_interactivity graph.
    std::shared_ptr<GLTF> withAnimation(const std::string &graph)
    {
        const std::string json = R"({"asset": {"version": "2.0"}, "extensionsUsed": ["KHR_interactivity"],
            "nodes": [{}],
            "buffers": [{"byteLength": 32, "uri": "data:application/octet-stream;base64,AAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="}],
            "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 8}, {"buffer": 0, "byteOffset": 8, "byteLength": 24}],
            "accessors": [{"bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0], "max": [2]},
                          {"bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC3"}],
            "animations": [{"channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}],
                            "samplers": [{"input": 0, "output": 1}]}],
            "extensions": {"KHR_interactivity": {"graph": 0, "graphs": [)" + graph + "]}}}";
        return GLTF::loadGLTF(json, kNoFiles);
    }

    const char *kTypes = R"("types": [{"signature": "bool"}, {"signature": "int"}, {"signature": "float"},
                                       {"signature": "float2"}, {"signature": "float3"}, {"signature": "ref"}])";
} // namespace

TEST(InteractivityRuntime, NoGraphIsNotValid)
{
    auto asset = GLTF::loadGLTF(R"({"asset": {"version": "2.0"}})", kNoFiles);
    ASSERT_NE(asset, nullptr);
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    EXPECT_FALSE(runtime.valid());
    runtime.start();
    runtime.tick(0.016);
    EXPECT_TRUE(host.sets.empty());
}

TEST(InteractivityRuntime, ForLoopSumsIntoAVariableAndSetsAPointer)
{
    // onStart -> for 0..5 (body: sum = sum + index) -> completed: /nodes/2/... = sum
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "variables": [{"type": 1, "value": [0]}],
        "declarations": [{"op": "event/onStart"}, {"op": "flow/for"}, {"op": "variable/set"},
                         {"op": "variable/get"}, {"op": "math/add"}, {"op": "pointer/set"}, {"op": "type/intToFloat"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 2}}},
          {"declaration": 3, "configuration": {"variable": {"value": [0]}}},
          {"declaration": 1, "values": {"startIndex": {"type": 1, "value": [0]}, "endIndex": {"type": 1, "value": [5]}},
           "flows": {"loopBody": {"node": 4}, "completed": {"node": 6}}},
          {"declaration": 4, "values": {"a": {"node": 1, "socket": "value"}, "b": {"node": 2, "socket": "index"}}},
          {"declaration": 2, "configuration": {"variables": {"value": [0]}}, "values": {"0": {"node": 3, "socket": "value"}}},
          {"declaration": 6, "values": {"a": {"node": 1, "socket": "value"}}},
          {"declaration": 5, "configuration": {"pointer": {"value": ["/nodes/2/extras/sum"]}, "type": {"value": [2]}},
           "values": {"value": {"node": 5, "socket": "value"}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity && asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    ASSERT_TRUE(runtime.valid());
    EXPECT_TRUE(runtime.unsupportedOperations().empty());
    runtime.start();
    ASSERT_TRUE(host.properties.count("/nodes/2/extras/sum"));
    EXPECT_DOUBLE_EQ(host.properties["/nodes/2/extras/sum"].f[0], 10.0); // 0+1+2+3+4
}

TEST(InteractivityRuntime, IntegerArithmeticWrapsAndDividesByZeroToZero)
{
    // onStart -> log "{a} {b} {c}" with a = 2147483647 + 1, b = 7 / 0, c = -7 rem 3
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onStart"}, {"op": "debug/log"}, {"op": "math/add"}, {"op": "math/div"}, {"op": "math/rem"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 4}}},
          {"declaration": 2, "values": {"a": {"type": 1, "value": [2147483647]}, "b": {"type": 1, "value": [1]}}},
          {"declaration": 3, "values": {"a": {"type": 1, "value": [7]}, "b": {"type": 1, "value": [0]}}},
          {"declaration": 4, "values": {"a": {"type": 1, "value": [-7]}, "b": {"type": 1, "value": [3]}}},
          {"declaration": 1, "configuration": {"message": {"value": ["{a} {b} {c} {{x}}"]}},
           "values": {"a": {"node": 1, "socket": "value"}, "b": {"node": 2, "socket": "value"}, "c": {"node": 3, "socket": "value"}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    runtime.start();
    ASSERT_EQ(host.logs.size(), 1u);
    EXPECT_EQ(host.logs[0], "-2147483648 0 -1 {x}");
}

TEST(InteractivityRuntime, SetDelayFiresAfterItsDuration)
{
    // onStart -> setDelay 0.5 s -> done: log "done"
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onStart"}, {"op": "flow/setDelay"}, {"op": "debug/log"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 1}}},
          {"declaration": 1, "values": {"duration": {"type": 2, "value": [0.5]}}, "flows": {"done": {"node": 2}}},
          {"declaration": 2, "configuration": {"message": {"value": ["done"]}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    runtime.start();
    runtime.tick(0.0);
    runtime.tick(0.3);
    EXPECT_TRUE(host.logs.empty());
    runtime.tick(0.3);
    ASSERT_EQ(host.logs.size(), 1u);
    EXPECT_EQ(host.logs[0], "done");
    runtime.tick(1.0);
    EXPECT_EQ(host.logs.size(), 1u);
}

TEST(InteractivityRuntime, EndlessLoopStopsAtTheBudget)
{
    // onStart -> while(true) { }
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onStart"}, {"op": "flow/while"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 1}}},
          {"declaration": 1, "values": {"condition": {"type": 0, "value": [true]}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    runtime.start();
    ASSERT_EQ(runtime.errors().size(), 1u);
    EXPECT_NE(runtime.errors()[0].find("stopped"), std::string::npos);
}

TEST(InteractivityRuntime, SelectPropagatesToAncestorsUnlessStopped)
{
    // onSelect on node 0 (the parent) logs "parent"; on node 1 logs "child".
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onSelect", "extension": "KHR_node_selectability",
                          "outputValueSockets": {"selectedNode": {"type": 5}, "controllerIndex": {"type": 1},
                                                 "selectionPoint": {"type": 4}, "selectionRayOrigin": {"type": 4}, "event": {"type": 5}}},
                         {"op": "debug/log"}],
        "nodes": [
          {"declaration": 0, "configuration": {"nodeIndex": {"value": [0]}}, "flows": {"out": {"node": 2}}},
          {"declaration": 0, "configuration": {"nodeIndex": {"value": [1]}}, "flows": {"out": {"node": 3}}},
          {"declaration": 1, "configuration": {"message": {"value": ["parent {n}"]}}, "values": {"n": {"node": 0, "socket": "selectedNode"}}},
          {"declaration": 1, "configuration": {"message": {"value": ["child"]}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error
                                                << (asset->khrInteractivity->graphs.empty() ? "" : asset->khrInteractivity->graphs[0].error);
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    EXPECT_TRUE(runtime.selectable(1));
    EXPECT_FALSE(runtime.selectable(2));
    EXPECT_TRUE(runtime.select(1));
    ASSERT_EQ(host.logs.size(), 2u);
    EXPECT_EQ(host.logs[0], "child");
    EXPECT_EQ(host.logs[1], "parent /nodes/1");
    EXPECT_FALSE(runtime.select(2));
}

TEST(InteractivityRuntime, QuaternionAndMatrixOperations)
{
    // r = rotate3D((1,0,0), quatFromAxisAngle((0,0,1), pi/2)) = (0,1,0)
    // m = matCompose((1,2,3), q, (2,2,2)); p = matMul(m, inverse(m)) = identity
    auto asset = withGraph(R"({"types": [{"signature": "bool"}, {"signature": "int"}, {"signature": "float"},
                                         {"signature": "float3"}, {"signature": "float4"}, {"signature": "float4x4"}],
        "declarations": [{"op": "math/quatFromAxisAngle"}, {"op": "math/rotate3D"}, {"op": "math/matCompose"},
                         {"op": "math/inverse"}, {"op": "math/matMul"}, {"op": "event/onStart"}, {"op": "pointer/set"}],
        "nodes": [
          {"declaration": 0, "values": {"axis": {"type": 3, "value": [0, 0, 1]}, "angle": {"type": 2, "value": [1.5707963267948966]}}},
          {"declaration": 1, "values": {"a": {"type": 3, "value": [1, 0, 0]}, "rotation": {"node": 0, "socket": "value"}}},
          {"declaration": 2, "values": {"translation": {"type": 3, "value": [1, 2, 3]}, "rotation": {"node": 0, "socket": "value"},
                                        "scale": {"type": 3, "value": [2, 2, 2]}}},
          {"declaration": 3, "values": {"a": {"node": 2, "socket": "value"}}},
          {"declaration": 4, "values": {"a": {"node": 2, "socket": "value"}, "b": {"node": 3, "socket": "value"}}},
          {"declaration": 5, "flows": {"out": {"node": 6}}},
          {"declaration": 6, "configuration": {"pointer": {"value": ["/nodes/0/extras/r"]}, "type": {"value": [3]}},
           "values": {"value": {"node": 1, "socket": "value"}}, "flows": {"out": {"node": 7}}},
          {"declaration": 6, "configuration": {"pointer": {"value": ["/nodes/0/extras/p"]}, "type": {"value": [5]}},
           "values": {"value": {"node": 4, "socket": "value"}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error
                                                << (asset->khrInteractivity->graphs.empty() ? "" : asset->khrInteractivity->graphs[0].error);
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    EXPECT_TRUE(runtime.unsupportedOperations().empty());
    runtime.start();
    const Value r = host.properties["/nodes/0/extras/r"];
    EXPECT_NEAR(r.f[0], 0.0, 1e-9);
    EXPECT_NEAR(r.f[1], 1.0, 1e-9);
    EXPECT_NEAR(r.f[2], 0.0, 1e-9);
    const Value p = host.properties["/nodes/0/extras/p"];
    ASSERT_EQ(p.type, Sig::Float4x4);
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) EXPECT_NEAR(p.f[c * 4 + row], c == row ? 1.0 : 0.0, 1e-9) << c << "," << row;
}

// ---- Calculator.glb (KhronosGroup/glTF-Test-Assets-Interactivity) --------------

namespace
{
    const fs::path kCalculator = fs::path("interactivity-assets") / "Models" / "Calculator" / "glTF-Binary" / "Calculator.glb";

    int64_t nodeNamed(const GLTF &asset, const std::string &name)
    {
        for (size_t i = 0; i < asset.nodes->size(); ++i)
            if ((*asset.nodes)[i].name == name) return static_cast<int64_t>(i);
        return -1;
    }
} // namespace

TEST(InteractivityRuntime, CalculatorButtonsUpdateTheDisplay)
{
    if (!fs::exists(kCalculator)) GTEST_SKIP() << "no " << kCalculator.string() << " (launch_test.sh clones the assets)";
    std::ifstream f(kCalculator, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto asset = GLTF::loadGLB(bytes.data(), bytes.size());
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity && asset->khrInteractivity->valid);

    FakeHost host;
    // The digit quads' texture transform scale (read by the graph).
    const std::string scale = "/pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/scale";
    const std::string offset = "/pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/offset";
    const double s[2] = {0.1, 0.5};
    host.properties["/materials/4" + scale] = Value::ofFloats(Sig::Float2, s);
    host.properties["/materials/5" + scale] = Value::ofFloats(Sig::Float2, s);

    InteractivityRuntime runtime(*asset, host);
    ASSERT_TRUE(runtime.valid());
    EXPECT_TRUE(runtime.unsupportedOperations().empty()) << runtime.unsupportedOperations().front();

    const int64_t nine = nodeNamed(*asset, "Button 9");
    const int64_t times = nodeNamed(*asset, "Button multiply");
    const int64_t minus = nodeNamed(*asset, "Button substract");
    const int64_t clear = nodeNamed(*asset, "Button clear");
    const int64_t negative = nodeNamed(*asset, "Negative");
    ASSERT_GE(nine, 0);
    ASSERT_GE(negative, 0);
    EXPECT_TRUE(runtime.selectable(nine));

    const auto tens = [&] { return host.properties["/materials/4" + offset].f[0]; };
    const auto units = [&] { return host.properties["/materials/5" + offset].f[0]; };
    const auto minusShown = [&] { return host.properties["/nodes/" + std::to_string(negative) + "/extensions/KHR_node_visibility/visible"].b; };

    runtime.start(); // shows 0
    EXPECT_NEAR(units(), 0.0, 1e-9);
    EXPECT_NEAR(tens(), 0.0, 1e-9);
    EXPECT_FALSE(minusShown());

    EXPECT_TRUE(runtime.select(nine)); // 9
    EXPECT_NEAR(units(), 0.9, 1e-9);
    EXPECT_NEAR(tens(), 0.0, 1e-9);

    EXPECT_TRUE(runtime.select(times)); // 18
    EXPECT_NEAR(units(), 0.8, 1e-9);
    EXPECT_NEAR(tens(), 0.1, 1e-9);

    EXPECT_TRUE(runtime.select(clear)); // 0
    EXPECT_TRUE(runtime.select(minus)); // -1
    EXPECT_NEAR(units(), 0.1, 1e-9);
    EXPECT_TRUE(minusShown());
    EXPECT_TRUE(runtime.errors().empty());
}

// ---- every valid test asset (robustness) ------------------------------------------

namespace
{
    // Accepts any pointer: reads give type defaults, writes succeed.
    class PermissiveHost : public InteractivityHost
    {
    public:
        bool getPointer(const std::string &, Sig type, Value &value) override
        {
            value = Value::defaultOf(type);
            return true;
        }
        bool setPointer(const std::string &, const Value &) override { return true; }
    };
} // namespace

TEST(InteractivityRuntime, RunsEveryValidTestAsset)
{
    const fs::path root("interactivity-assets");
    if (!fs::is_directory(root)) GTEST_SKIP() << "no " << root.string() << " (launch_test.sh clones the assets)";
    const fs::path invalidDir = root / "Tests" / "Interactivity" / "invalid";
    std::map<std::string, int> unsupported;
    int ran = 0;
    for (const auto &entry : fs::recursive_directory_iterator(root))
    {
        const auto ext = entry.path().extension();
        if (!entry.is_regular_file() || (ext != ".gltf" && ext != ".glb")) continue;
        const auto rel = entry.path().lexically_relative(invalidDir);
        if (!rel.empty() && *rel.begin() != "..") continue; // an invalid asset
        std::ifstream f(entry.path(), std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::shared_ptr<GLTF> asset;
        const fs::path dir = entry.path().parent_path();
        try
        {
            asset = ext == ".glb" ? GLTF::loadGLB(bytes.data(), bytes.size())
                                  : GLTF::loadGLTF(std::string(bytes.begin(), bytes.end()), [dir](const std::string &uri) {
                                        return std::async(std::launch::deferred, [dir, uri]() {
                                            std::ifstream in(dir / fs::u8path(uri), std::ios::binary);
                                            return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                                        });
                                    });
        }
        catch (const std::exception &)
        {
            continue;
        }
        if (!asset || !asset->khrInteractivity) continue;
        PermissiveHost host;
        InteractivityRuntime runtime(*asset, host);
        if (!runtime.valid()) continue;
        if (std::getenv("CWV_RUNTIME_TRACE")) { std::printf("%s\n", entry.path().string().c_str()); std::fflush(stdout); }
        runtime.start();
        for (int t = 0; t < 10; ++t) runtime.tick(1.0 / 60.0);
        if (asset->nodes)
            for (size_t n = 0; n < asset->nodes->size(); ++n) runtime.select(static_cast<int64_t>(n));
        for (const auto &op : runtime.unsupportedOperations()) ++unsupported[op];
        ++ran;
    }
    EXPECT_GT(ran, 100);
    std::string list;
    for (const auto &[op, count] : unsupported) list += " " + op + " (" + std::to_string(count) + ")";
    std::printf("[runtime] ran %d assets; no-op operations:%s\n", ran, list.empty() ? " none" : list.c_str());
}

namespace
{
    const char *kTypes4 = R"("types": [{"signature": "bool"}, {"signature": "int"}, {"signature": "float"},
                                        {"signature": "float2"}, {"signature": "float3"}, {"signature": "ref"},
                                        {"signature": "float4"}])";
    // Linear easing: control points on the diagonal.
    const std::string kLinear = R"("p1": {"type": 3, "value": [0, 0]}, "p2": {"type": 3, "value": [1, 1]})";
} // namespace

TEST(InteractivityRuntime, VariableInterpolateEasesToTheTargetThenFiresDone)
{
    // onStart -> interpolate v from 0 to 4 in 2 s -> done: log "done"; onTick: log v
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "variables": [{"type": 2, "value": [0]}],
        "declarations": [{"op": "event/onStart"}, {"op": "variable/interpolate"}, {"op": "debug/log"},
                         {"op": "event/onTick"}, {"op": "variable/get"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 1}}},
          {"declaration": 1, "configuration": {"variable": {"value": [0]}, "useSlerp": {"value": [false]}},
           "values": {"value": {"type": 2, "value": [4]}, "duration": {"type": 2, "value": [2]}, )" + kLinear + R"(},
           "flows": {"done": {"node": 2}}},
          {"declaration": 2, "configuration": {"message": {"value": ["done"]}}},
          {"declaration": 4, "configuration": {"variable": {"value": [0]}}},
          {"declaration": 3, "flows": {"out": {"node": 5}}},
          {"declaration": 2, "configuration": {"message": {"value": ["{v}"]}}, "values": {"v": {"node": 3, "socket": "value"}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    EXPECT_TRUE(runtime.unsupportedOperations().empty());
    runtime.start();
    EXPECT_TRUE(runtime.animating());
    runtime.tick(0.0);
    runtime.tick(0.5);
    runtime.tick(1.5);
    EXPECT_EQ(host.logs, (std::vector<std::string>{"0", "1", "done", "4"}));
    EXPECT_FALSE(runtime.animating());
}

TEST(InteractivityRuntime, PointerInterpolateSlerpsRotations)
{
    // onStart -> interpolate /nodes/0/rotation to a half turn about Y in 1 s
    // -> out: interpolate a missing property (err: log "err"); done: log "done"
    auto asset = withGraph(std::string("{") + kTypes4 + R"(,
        "declarations": [{"op": "event/onStart"}, {"op": "pointer/interpolate"}, {"op": "debug/log"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 1}}},
          {"declaration": 1, "configuration": {"pointer": {"value": ["/nodes/0/rotation"]}, "type": {"value": [6]}},
           "values": {"value": {"type": 6, "value": [0, 1, 0, 0]}, "duration": {"type": 2, "value": [1]}, )" + kLinear + R"(},
           "flows": {"out": {"node": 2}, "done": {"node": 3}}},
          {"declaration": 1, "configuration": {"pointer": {"value": ["/nodes/0/missing"]}, "type": {"value": [2]}},
           "values": {"value": {"type": 2, "value": [1]}, "duration": {"type": 2, "value": [1]}, )" + kLinear + R"(},
           "flows": {"err": {"node": 4}}},
          {"declaration": 2, "configuration": {"message": {"value": ["done"]}}},
          {"declaration": 2, "configuration": {"message": {"value": ["err"]}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    const double identity[4] = {0, 0, 0, 1};
    host.properties["/nodes/0/rotation"] = Value::ofFloats(Sig::Float4, identity);
    InteractivityRuntime runtime(*asset, host);
    runtime.start();
    EXPECT_EQ(host.logs, (std::vector<std::string>{"err"}));
    runtime.tick(0.0);
    runtime.tick(0.5);
    const auto &q = host.properties["/nodes/0/rotation"].f;
    EXPECT_NEAR(q[1], std::sqrt(0.5), 1e-6); // slerp; a lerp gives 0.5
    EXPECT_NEAR(q[3], std::sqrt(0.5), 1e-6);
    runtime.tick(0.5);
    EXPECT_DOUBLE_EQ(host.properties["/nodes/0/rotation"].f[1], 1.0);
    EXPECT_EQ(host.logs, (std::vector<std::string>{"err", "done"}));
}

TEST(InteractivityRuntime, AnimationStartPlaysToItsEndTimeAndWraps)
{
    // onStart -> start animation 0 from 0 to 5 s (it lasts 2 s: it repeats)
    // -> done: log "ended {playhead}"
    auto asset = withAnimation(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onStart"}, {"op": "animation/start"}, {"op": "debug/log"}, {"op": "pointer/get"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 1}}},
          {"declaration": 1, "values": {"animation": {"type": 5, "value": ["/animations/0"]}, "startTime": {"type": 2, "value": [0]},
                                        "endTime": {"type": 2, "value": [5]}, "speed": {"type": 2, "value": [1]}},
           "flows": {"done": {"node": 3}}},
          {"declaration": 3, "configuration": {"pointer": {"value": ["/animations/0/extensions/KHR_interactivity/playhead"]}, "type": {"value": [2]}}},
          {"declaration": 2, "configuration": {"message": {"value": ["ended {p}"]}}, "values": {"p": {"node": 2, "socket": "value"}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    EXPECT_TRUE(runtime.unsupportedOperations().empty());
    runtime.start();
    runtime.tick(0.0);
    runtime.tick(3.0); // 3 s: 1 s into the second repetition
    runtime.tick(2.5); // past 5 s, the end time
    ASSERT_EQ(host.applied.size(), 3u);
    EXPECT_DOUBLE_EQ(host.applied[0].second, 0.0);
    EXPECT_DOUBLE_EQ(host.applied[1].second, 1.0);
    EXPECT_DOUBLE_EQ(host.applied[2].second, 1.0); // 5 s is 1 s into the third
    EXPECT_EQ(host.stopped, (std::vector<int64_t>{0}));
    EXPECT_EQ(host.logs, (std::vector<std::string>{"ended 1"}));
    EXPECT_FALSE(runtime.animating());
}

TEST(InteractivityRuntime, AnimationStopAtEndsEarlyWithItsOwnDone)
{
    // onStart -> start animation 0, 0 to 5 s at speed 2 -> out: stopAt 1 s;
    // start's done logs "ended", stopAt's logs "stopped"
    auto asset = withAnimation(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onStart"}, {"op": "animation/start"}, {"op": "animation/stopAt"}, {"op": "debug/log"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 1}}},
          {"declaration": 1, "values": {"animation": {"type": 5, "value": ["/animations/0"]}, "startTime": {"type": 2, "value": [0]},
                                        "endTime": {"type": 2, "value": [5]}, "speed": {"type": 2, "value": [2]}},
           "flows": {"out": {"node": 2}, "done": {"node": 3}}},
          {"declaration": 2, "values": {"animation": {"type": 5, "value": ["/animations/0"]}, "stopTime": {"type": 2, "value": [1]}},
           "flows": {"done": {"node": 4}}},
          {"declaration": 3, "configuration": {"message": {"value": ["ended"]}}},
          {"declaration": 3, "configuration": {"message": {"value": ["stopped"]}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    runtime.start();
    runtime.tick(0.0);
    runtime.tick(1.0); // 2 s of animation: past the stop time
    ASSERT_EQ(host.applied.size(), 2u);
    EXPECT_DOUBLE_EQ(host.applied[1].second, 1.0);
    EXPECT_EQ(host.logs, (std::vector<std::string>{"stopped"}));
    runtime.tick(5.0);
    EXPECT_EQ(host.applied.size(), 2u);
}

TEST(InteractivityRuntime, HoverInAndOutStopBelowTheCommonAncestor)
{
    // Node 0 (parent of 1) logs "in {n}" / "out {n}"; node 2 logs "in2".
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onHoverIn", "extension": "KHR_node_hoverability",
                          "outputValueSockets": {"hoveredNode": {"type": 5}, "controllerIndex": {"type": 1}, "event": {"type": 5}}},
                         {"op": "event/onHoverOut", "extension": "KHR_node_hoverability",
                          "outputValueSockets": {"hoveredNode": {"type": 5}, "controllerIndex": {"type": 1}, "event": {"type": 5}}},
                         {"op": "debug/log"}],
        "nodes": [
          {"declaration": 0, "configuration": {"nodeIndex": {"value": [0]}}, "flows": {"out": {"node": 3}}},
          {"declaration": 1, "configuration": {"nodeIndex": {"value": [0]}}, "flows": {"out": {"node": 4}}},
          {"declaration": 0, "configuration": {"nodeIndex": {"value": [2]}}, "flows": {"out": {"node": 5}}},
          {"declaration": 2, "configuration": {"message": {"value": ["in {n}"]}}, "values": {"n": {"node": 0, "socket": "hoveredNode"}}},
          {"declaration": 2, "configuration": {"message": {"value": ["out {n}"]}}, "values": {"n": {"node": 1, "socket": "hoveredNode"}}},
          {"declaration": 2, "configuration": {"message": {"value": ["in2"]}}}
        ]})");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    EXPECT_TRUE(runtime.unsupportedOperations().empty());
    EXPECT_TRUE(runtime.usesHover());
    EXPECT_TRUE(runtime.hover(1));
    EXPECT_FALSE(runtime.hover(0)); // still within node 0's subtree
    EXPECT_FALSE(runtime.hover(0));
    EXPECT_TRUE(runtime.hover(2));
    EXPECT_FALSE(runtime.hover(-1)); // node 2 has no onHoverOut
    EXPECT_EQ(host.logs, (std::vector<std::string>{"in /nodes/1", "out /nodes/0", "in2"}));
}

TEST(InteractivityRuntime, GhostPlaysItsAnimationsOnHover)
{
    // Hovering the ghost plays "scared" (animation 0) to its end; leaving it
    // stops that and plays "scared-reversed" (1). The graph reads both through
    // pointer/get "/animations/[index]" (a ref) and their maxTime.
    const fs::path ghost = fs::path("interactivity-assets") / "Models" / "Ghost" / "glTF-Binary" / "ghost_v2.glb";
    if (!fs::exists(ghost)) GTEST_SKIP() << "no " << ghost.string() << " (launch_test.sh clones the assets)";
    std::ifstream f(ghost, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto asset = GLTF::loadGLB(bytes.data(), bytes.size());
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity && asset->khrInteractivity->valid);

    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    ASSERT_TRUE(runtime.valid());
    EXPECT_TRUE(runtime.unsupportedOperations().empty()) << runtime.unsupportedOperations().front();
    EXPECT_TRUE(runtime.usesHover());
    runtime.start();
    runtime.tick(0.0);
    EXPECT_TRUE(host.applied.empty()); // nothing plays by itself

    EXPECT_TRUE(runtime.hover(4));
    EXPECT_TRUE(runtime.animating());
    runtime.tick(0.1);
    ASSERT_FALSE(host.applied.empty());
    EXPECT_EQ(host.applied.back().first, 0);
    EXPECT_NEAR(host.applied.back().second, 0.1, 1e-9);
    for (int i = 0; i < 100; ++i) runtime.tick(0.1); // past its end
    EXPECT_FALSE(runtime.animating());
    EXPECT_EQ(host.stopped, (std::vector<int64_t>{0}));

    EXPECT_TRUE(runtime.hover(-1));
    runtime.tick(0.1);
    EXPECT_EQ(host.applied.back().first, 1);
    EXPECT_TRUE(runtime.errors().empty());
}

TEST(InteractivityRuntime, PointerGetAnswersTheAssetStructure)
{
    // The glTF Asset Object Model's read-only structure, from the asset:
    // array sizes (".length") and references (parent, children, mesh). A
    // reference that doesn't exist (a root's parent, a node without a mesh)
    // isn't valid. Each value is copied to a host property on start.
    auto get = [](const char *pointer, int type) {
        return std::string(R"({"declaration": 2, "configuration": {"pointer": {"value": [")") + pointer +
               R"("]}, "type": {"value": [)" + std::to_string(type) + "]}}}";
    };
    auto set = [](const char *name, int type, int node, const char *socket) {
        return std::string(R"({"declaration": 3, "configuration": {"pointer": {"value": ["/extras/)") + name +
               R"("]}, "type": {"value": [)" + std::to_string(type) + R"(]}}, "values": {"value": {"node": )" +
               std::to_string(node) + R"(, "socket": ")" + socket + R"("}}})";
    };
    auto asset = withGraph(std::string("{") + kTypes + R"(,
        "declarations": [{"op": "event/onStart"}, {"op": "flow/sequence"}, {"op": "pointer/get"}, {"op": "pointer/set"}],
        "nodes": [
          {"declaration": 0, "flows": {"out": {"node": 1}}},
          {"declaration": 1, "flows": {"a": {"node": 8}, "b": {"node": 9}, "c": {"node": 10}, "d": {"node": 11},
                                       "e": {"node": 12}, "f": {"node": 13}}},
          )" + get("/nodes.length", 1) + "," + get("/nodes/0/children.length", 1) + "," +
                               get("/nodes/1/parent", 5) + "," + get("/nodes/0/parent", 5) + "," +
                               get("/nodes/0/mesh", 5) + "," + get("/nodes/0/children/0", 5) + "," +
                               set("count", 1, 2, "value") + "," + set("children", 1, 3, "value") + "," +
                               set("parent", 5, 4, "value") + "," + set("rootParent", 0, 5, "isValid") + "," +
                               set("mesh", 0, 6, "isValid") + "," + set("child", 5, 7, "value") + "]}");
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->khrInteractivity && asset->khrInteractivity->valid) << asset->khrInteractivity->error;
    FakeHost host;
    InteractivityRuntime runtime(*asset, host);
    ASSERT_TRUE(runtime.valid());
    runtime.start();
    EXPECT_TRUE(runtime.errors().empty());
    ASSERT_TRUE(host.properties.count("/extras/count"));
    EXPECT_EQ(host.properties["/extras/count"].i, 3);
    EXPECT_EQ(host.properties["/extras/children"].i, 1);
    EXPECT_EQ(host.properties["/extras/parent"].ref.collection, "nodes");
    EXPECT_EQ(host.properties["/extras/parent"].ref.index, 0);
    EXPECT_FALSE(host.properties["/extras/rootParent"].b);
    EXPECT_FALSE(host.properties["/extras/mesh"].b);
    EXPECT_EQ(host.properties["/extras/child"].ref.index, 1);
}
