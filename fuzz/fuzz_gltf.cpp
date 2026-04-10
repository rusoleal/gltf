/**
 * libFuzzer entry point for GLTF::loadGLTF (JSON path).
 *
 * Build with:
 *   cmake -B build -S . -DGLTF_BUILD_FUZZ=ON
 *   make -C build gltf_fuzz_gltf
 *
 * Run:
 *   ./build/gltf_fuzz_gltf fuzz/corpus/gltf/ -max_total_time=60
 */

#include <cstdint>
#include <future>
#include <stdexcept>
#include <string>
#include <vector>
#include <gltf/gltf.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    try
    {
        std::string json(reinterpret_cast<const char *>(data), size);
        // Fuzzing uses no-op callback since we're testing the parser, not external loading
        auto gltf = systems::leal::gltf::GLTF::loadGLTF(json, [](const std::string &) {
            return std::async(std::launch::deferred, []() {
                return std::vector<uint8_t>{};
            });
        });

        // Also exercise getRuntimeInfo so the traversal code is fuzzed.
        if (gltf && gltf->scenes && !gltf->scenes->empty())
        {
            try { gltf->getRuntimeInfo(0); } catch (const std::exception &) {}
        }
    }
    catch (const std::exception &)
    {
        // Expected for malformed input — not a bug.
    }
    catch (...)
    {
        // Non-std exceptions should not escape; treat as a finding.
        return 1;
    }
    return 0;
}
