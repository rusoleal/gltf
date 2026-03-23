@echo off
setlocal

cmake -B build -S . -DGLTF_BUILD_TEST=ON
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build build
if %errorlevel% neq 0 exit /b %errorlevel%

set BASE=https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/refs/heads/main/Models

rem Core test assets
curl "%BASE%/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf"                      -o build/AlphaBlendModeTest.gltf
curl "%BASE%/AlphaBlendModeTest/glTF-Embedded/AlphaBlendModeTest.gltf"             -o build/AlphaBlendModeTestEmbedded.gltf
curl "%BASE%/AlphaBlendModeTest/glTF-Binary/AlphaBlendModeTest.glb"                -o build/AlphaBlendModeTest.glb

rem Extension integration test assets
curl "%BASE%/LightsPunctualLamp/glTF/LightsPunctualLamp.gltf"                      -o build/LightsPunctualLamp.gltf
curl "%BASE%/AnimationPointerUVs/glTF/AnimationPointerUVs.gltf"                    -o build/AnimationPointerUVs.gltf
curl "%BASE%/MaterialsVariantsShoe/glTF/MaterialsVariantsShoe.gltf"                -o build/MaterialsVariantsShoe.gltf

rem KHR_draco_mesh_compression - Avocado (Draco variant + its binary buffer)
curl "%BASE%/Avocado/glTF-Draco/Avocado.gltf"                                      -o build/AvocadoDraco.gltf
curl "%BASE%/Avocado/glTF-Draco/Avocado.bin"                                       -o build/Avocado.bin

rem KHR_meshopt_compression - BrainStem (Meshopt variant + its binary buffer)
curl "%BASE%/BrainStem/glTF-Meshopt/BrainStem.gltf"                               -o build/BrainStem.gltf
curl "%BASE%/BrainStem/glTF-Meshopt/BrainStem.bin"                                -o build/BrainStem.bin

rem Run tests - check both flat and config-specific output dirs (Debug/Release)
if exist build\gltf_test.exe (
    build\gltf_test.exe
) else if exist build\Debug\gltf_test.exe (
    build\Debug\gltf_test.exe
) else if exist build\Release\gltf_test.exe (
    build\Release\gltf_test.exe
) else (
    echo ERROR: gltf_test.exe not found in build, build\Debug, or build\Release
    exit /b 1
)
