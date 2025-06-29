cmake -B build -S . -DGLTF_BUILD_TEST=ON
make -C build
curl https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf > build/AlphaBlendModeTest.gltf
curl https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF-Embedded/AlphaBlendModeTest.gltf > build/AlphaBlendModeTestEmbedded.gltf
curl https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models/AlphaBlendModeTest/glTF-Binary/AlphaBlendModeTest.glb > build/AlphaBlendModeTest.glb
cd build
./gltf_test
cd ..