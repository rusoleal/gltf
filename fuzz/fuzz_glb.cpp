/**
 * libFuzzer entry point for GLTF::loadGLB (binary GLB path).
 *
 * Build with:
 *   cmake -B build -S . -DGLTF_BUILD_FUZZ=ON
 *   make -C build gltf_fuzz_glb
 *
 * Run:
 *   ./build/gltf_fuzz_glb fuzz/corpus/glb/ -max_total_time=60
 */

#include <cstdint>
#include <stdexcept>
#include <gltf/gltf.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    try
    {
        // Cast away const: loadGLB takes a non-const pointer but does not
        // modify the buffer, so this is safe for fuzzing purposes.
        auto gltf = systems::leal::gltf::GLTF::loadGLB(
            const_cast<uint8_t *>(data), static_cast<uint64_t>(size));

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
        return 1;
    }
    return 0;
}
