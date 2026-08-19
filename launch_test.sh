cmake -B build -S . -DGLTF_BUILD_TEST=ON
make -C build

BASE=https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models

# Core test assets
curl "$BASE/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf"                      > build/AlphaBlendModeTest.gltf
curl "$BASE/AlphaBlendModeTest/glTF-Binary/AlphaBlendModeTest.glb"                > build/AlphaBlendModeTest.glb

# glTF 2.1 local fixtures (until official Khronos sample assets are published)
mkdir -p build/gltf21
cp test/assets/gltf21/*.gltf build/gltf21/

# glTF 2.1 official sample assets (optional — skips gracefully while Khronos
# has not yet published a public 2.1 sample set).
mkdir -p build/gltf21_official

# Override this env var to point at a directory or manifest of official 2.1 assets.
GLTF21_SAMPLE_REPO="${GLTF21_SAMPLE_REPO:-}"
if [ -n "$GLTF21_SAMPLE_REPO" ]; then
    echo "Attempting to download official glTF 2.1 sample assets from $GLTF21_SAMPLE_REPO ..."
    # If the env var points to a raw JSON manifest, download each listed asset.
    if curl -fsSL "$GLTF21_SAMPLE_REPO" > build/gltf21_official/manifest.json 2>/dev/null; then
        # Minimal jq-free extraction of "url" or "uri" fields from a JSON manifest.
        python3 - <<'PY' 2>/dev/null || true
import json, os, urllib.request, sys
manifest = "build/gltf21_official/manifest.json"
if not os.path.exists(manifest):
    sys.exit(0)
with open(manifest) as f:
    data = json.load(f)
items = data if isinstance(data, list) else data.get("assets", [])
for item in items:
    url = item.get("url") or item.get("uri") or item.get("download_url")
    if not url:
        continue
    out = os.path.join("build/gltf21_official", os.path.basename(url))
    try:
        urllib.request.urlretrieve(url, out)
        print("Downloaded", out)
    except Exception as e:
        print("Skipped", url, e)
PY
    fi
else
    echo "No GLTF21_SAMPLE_REPO set; skipping official glTF 2.1 asset downloads."
    echo "Set GLTF21_SAMPLE_REPO to a manifest URL or local directory to enable them."
fi

# Extension integration test assets
curl "$BASE/LightsPunctualLamp/glTF/LightsPunctualLamp.gltf"                      > build/LightsPunctualLamp.gltf
curl "$BASE/AnimationPointerUVs/glTF/AnimationPointerUVs.gltf"                     > build/AnimationPointerUVs.gltf
curl "$BASE/MaterialsVariantsShoe/glTF/MaterialsVariantsShoe.gltf"                > build/MaterialsVariantsShoe.gltf

# KHR_draco_mesh_compression — Avocado (Draco variant + its binary buffer)
curl "$BASE/Avocado/glTF-Draco/Avocado.gltf"                                      > build/AvocadoDraco.gltf
curl "$BASE/Avocado/glTF-Draco/Avocado.bin"                                        > build/Avocado.bin

# KHR_meshopt_compression — BrainStem (Meshopt variant + its binary buffer)
curl "$BASE/BrainStem/glTF-Meshopt/BrainStem.gltf"                                > build/BrainStem.gltf
curl "$BASE/BrainStem/glTF-Meshopt/BrainStem.bin"                                 > build/BrainStem.bin

cd build
./gltf_test
cd ..