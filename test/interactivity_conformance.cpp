/**
 * KHR_interactivity conformance against the Khronos test assets
 * (https://github.com/KhronosGroup/glTF-Test-Assets-Interactivity).
 *
 * Expects a clone at build/interactivity-assets (launch_test.sh makes one);
 * skips when it is not there.
 *
 * - Every asset outside Tests/Interactivity/invalid must load, with its graphs valid.
 * - Every asset in Tests/Interactivity/invalid breaks one rule and must be rejected.
 */

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gltf/gltf.hpp>

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
