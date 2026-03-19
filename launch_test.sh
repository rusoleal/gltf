cmake -B build -S . -DGLTF_BUILD_TEST=ON
make -C build

BASE=https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models

# Core test assets
curl "$BASE/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf"                      > build/AlphaBlendModeTest.gltf
curl "$BASE/AlphaBlendModeTest/glTF-Embedded/AlphaBlendModeTest.gltf"             > build/AlphaBlendModeTestEmbedded.gltf
curl "$BASE/AlphaBlendModeTest/glTF-Binary/AlphaBlendModeTest.glb"                > build/AlphaBlendModeTest.glb

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