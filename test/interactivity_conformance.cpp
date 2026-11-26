/**
 * KHR_interactivity conformance against the Khronos test assets
 * (https://github.com/KhronosGroup/glTF-Test-Assets-Interactivity).
 *
 * Expects a clone at build/interactivity-assets (launch_test.sh makes one);
 * skips when it is not there.
 *
 * - Every asset outside Tests/Interactivity/invalid must load, with its graphs valid.
 * - Every asset in Tests/Interactivity/invalid breaks one rule and must be rejected.
 * - The self-checking tests run and succeed (those that don't need an
 *   application's object model).
 */

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <future>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>
#include <gltf/extensions/khr_interactivity_runtime.hpp>
#include "../src/json.hpp" // the test indexes

using namespace systems::leal::gltf;
namespace fs = std::filesystem;

namespace
{
    const fs::path kAssetsRoot("interactivity-assets");

    std::vector<uint8_t> readBytes(const fs::path &path)
    {
        std::ifstream f(path, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    std::shared_ptr<GLTF> loadAsset(const fs::path &path)
    {
        auto bytes = readBytes(path);
        if (path.extension() == ".glb")
            return GLTF::loadGLB(bytes.data(), bytes.size());

        const fs::path dir = path.parent_path();
        return GLTF::loadGLTF(std::string(bytes.begin(), bytes.end()), [dir](const std::string &uri) {
            return std::async(std::launch::deferred, [dir, uri]() { return readBytes(dir / fs::u8path(uri)); });
        });
    }

    std::vector<fs::path> collect(bool invalid)
    {
        std::vector<fs::path> out;
        if (!fs::is_directory(kAssetsRoot))
            return out;
        const fs::path invalidDir = kAssetsRoot / "Tests" / "Interactivity" / "invalid";
        for (const auto &entry : fs::recursive_directory_iterator(kAssetsRoot))
        {
            if (!entry.is_regular_file())
                continue;
            const auto ext = entry.path().extension();
            if (ext != ".gltf" && ext != ".glb")
                continue;
            const auto rel = entry.path().lexically_relative(invalidDir);
            const bool isInvalid = !rel.empty() && *rel.begin() != "..";
            if (isInvalid == invalid)
                out.push_back(entry.path());
        }
        return out;
    }

    // Why an asset's interactivity was rejected; empty when it was accepted.
    std::string rejection(const fs::path &path)
    {
        std::shared_ptr<GLTF> asset;
        try
        {
            asset = loadAsset(path);
        }
        catch (const std::exception &e)
        {
            return std::string("load threw: ") + e.what();
        }
        if (!asset || !asset->khrInteractivity)
            return "no KHR_interactivity";
        const auto &ext = *asset->khrInteractivity;
        if (!ext.valid)
            return "extension rejected: " + ext.error;
        const auto &graph = ext.graphs[ext.graph];
        if (!graph.valid)
            return "default graph rejected: " + graph.error;
        return "";
    }
}

TEST(KHRInteractivityConformance, ValidAssetsAccepted)
{
    const auto files = collect(false);
    if (files.empty())
        GTEST_SKIP() << "No interactivity assets; clone glTF-Test-Assets-Interactivity into build/interactivity-assets.";

    std::ostringstream failures;
    size_t failed = 0;
    for (const auto &file : files)
    {
        const auto why = rejection(file);
        if (!why.empty())
        {
            ++failed;
            failures << "  " << file.generic_string() << ": " << why << "\n";
        }
    }
    EXPECT_EQ(failed, 0u) << failed << " of " << files.size() << " valid assets rejected:\n" << failures.str();
}

TEST(KHRInteractivityConformance, InvalidAssetsRejected)
{
    const auto files = collect(true);
    if (files.empty())
        GTEST_SKIP() << "No interactivity assets; clone glTF-Test-Assets-Interactivity into build/interactivity-assets.";

    // Set GLTF_INTERACTIVITY_VERBOSE to print why each case was rejected.
    const bool verbose = std::getenv("GLTF_INTERACTIVITY_VERBOSE") != nullptr;
    std::ostringstream failures;
    size_t failed = 0;
    for (const auto &file : files)
    {
        const auto why = rejection(file);
        if (verbose)
            std::cout << file.filename().generic_string() << ": " << why << "\n";
        if (why.empty())
        {
            ++failed;
            failures << "  " << file.filename().generic_string() << "\n";
        }
    }
    EXPECT_EQ(failed, 0u) << failed << " of " << files.size() << " invalid assets accepted:\n" << failures.str();
}

// ---- the self-checking tests, run ------------------------------------------------

namespace
{
    // The glTF Asset Object Model as a map: pointer/set stores, pointer/get
    // answers what was stored (nothing else). Enough for the tests of the
    // runtime itself; those that read the asset's own properties (animations,
    // lights, matrices) need an application's object model.
    class SelfCheckHost : public InteractivityHost
    {
    public:
        explicit SelfCheckHost(const InteractivityGraph &graph) : graph_(graph) {}
        std::map<std::string, InteractivityRuntimeValue> properties;
        std::string result; // "test/onSuccess" or "test/onFailed" once sent
        std::vector<std::string> logs;

        bool getPointer(const std::string &pointer, InteractivityTypeSignature type, InteractivityRuntimeValue &value) override
        {
            const auto it = properties.find(pointer);
            if (it == properties.end() || it->second.type != type) return false;
            value = it->second;
            return true;
        }
        bool setPointer(const std::string &pointer, const InteractivityRuntimeValue &value) override
        {
            properties[pointer] = value;
            return true;
        }
        void log(const std::string &message) override { logs.push_back(message); }
        void eventSent(int64_t eventIndex, const std::map<std::string, InteractivityRuntimeValue> &) override
        {
            const std::string &id = graph_.events[static_cast<size_t>(eventIndex)].id;
            if (result.empty() && (id == "test/onSuccess" || id == "test/onFailed")) result = id;
        }

    private:
        const InteractivityGraph &graph_;
    };

    // Cases that read the asset's own properties through pointer/get (or play
    // its animations): they need an application's object model.
    const std::set<std::string> kNeedObjectModel = {
        "Extras/Matrix_Updates",
        "animation/start",
        "animation/start_playback_modes",
        "animation/state",
        "animation/stop",
        "animation/stop_and_stopAt_edge_cases",
        "animation/stopAt",
        "math/matDecomp-Shear", // /nodes/N/globalMatrix
        "pointer/activeCamera",
        "pointer/get_light_type_pointers",
        "pointer/get_set_morphtargets",
        "pointer/interpolate",
        "pointer/set_edge_cases",
    };
}

// Khronos' self-checking tests (Tests/Interactivity, test-index.json and
// mathtests-index.json) send test/onStart, then test/onSuccess or
// test/onFailed: each must succeed within 20 simulated seconds.
TEST(KHRInteractivityConformance, SelfCheckingTestsSucceed)
{
    const fs::path root = kAssetsRoot / "Tests" / "Interactivity";
    if (!fs::is_directory(root))
        GTEST_SKIP() << "No interactivity assets; clone glTF-Test-Assets-Interactivity into build/interactivity-assets.";

    std::ostringstream failures;
    size_t ran = 0, failed = 0;
    for (const char *index : {"test-index.json", "mathtests-index.json"})
    {
        std::ifstream in(root / index);
        ASSERT_TRUE(in) << index;
        const auto entries = nlohmann::json::parse(in);
        for (const auto &entry : entries)
        {
            const auto variant = entry.find("variants");
            if (variant == entry.end() || !variant->contains("glTF-Binary")) continue;
            const std::string name = entry.at("name");
            const auto asset = loadAsset(root / name / "glTF-Binary" / (*variant)["glTF-Binary"].get<std::string>());
            ASSERT_TRUE(asset && asset->khrInteractivity) << name;
            const auto &ext = *asset->khrInteractivity;
            SelfCheckHost host(ext.graphs[ext.graph]);
            InteractivityRuntime runtime(*asset, host);
            ASSERT_TRUE(runtime.valid()) << name;
            runtime.start();
            for (int t = 0; t < 20 * 60 && host.result.empty(); ++t) runtime.tick(1.0 / 60.0);
            ++ran;
            const bool passed = host.result == "test/onSuccess";
            const bool known = kNeedObjectModel.count(name) != 0;
            if (passed == known)
            {
                ++failed;
                failures << "  " << name << ": " << (known ? "passes; remove it from kNeedObjectModel" : host.result.empty() ? "no result" : host.result);
                for (const auto &line : host.logs)
                    if (line.rfind("ERROR!", 0) == 0) failures << "\n    " << line;
                failures << "\n";
            }
        }
    }
    EXPECT_GT(ran, 150u);
    EXPECT_EQ(failed, 0u) << failures.str();
}
