dxc.exe -T vs_6_0 -E simpleVS -spirv -fspv-debug=vulkan-with-source -Fo SimpleShader_simpleVS.spirv SimpleShader.hlsl > SimpleShader_simpleVS.log 2>&1
dxc.exe -T ps_6_0 -E simplePS -spirv -fspv-debug=vulkan-with-source -Fo SimpleShader_simplePS.spirv SimpleShader.hlsl > SimpleShader_simplePS.log 2>&1

dxc.exe -T vs_6_0 -E screenSpaceVS -spirv -fspv-debug=vulkan-with-source -Fo ScreenSpace_screenSpaceVS.spirv ScreenSpace.hlsl > ScreenSpace_screenSpaceVS.log 2>&1
dxc.exe -T ps_6_0 -E toneMapPS -spirv -fspv-debug=vulkan-with-source -Fo ScreenSpace_toneMapPS.spirv ScreenSpace.hlsl > ScreenSpace_toneMapPS.log 2>&1
dxc.exe -T ps_6_0 -E passThroughPS -spirv -fspv-debug=vulkan-with-source -Fo ScreenSpace_passThroughPS.spirv ScreenSpace.hlsl > ScreenSpace_passThroughPS.log 2>&1

dxc.exe -T vs_6_0 -E skyboxVS -spirv -fspv-debug=vulkan-with-source -Fo Skybox_skyboxVS.spirv Skybox.hlsl > Skybox_skyboxVS.log 2>&1
dxc.exe -T ps_6_0 -E skyboxPS -spirv -fspv-debug=vulkan-with-source -Fo Skybox_skyboxPS.spirv Skybox.hlsl > Skybox_skyboxPS.log 2>&1

dxc.exe -T vs_6_0 -E skyVS -spirv -fspv-debug=vulkan-with-source -Fo Sky_skyVS.spirv Sky.hlsl > Sky_skyVS.log 2>&1
dxc.exe -T ps_6_0 -E skyPS -spirv -fspv-debug=vulkan-with-source -Fo Sky_skyPS.spirv Sky.hlsl > Sky_skyPS.log 2>&1

dxc.exe -T vs_6_0 -E irradianceVS -spirv -fspv-debug=vulkan-with-source -Fo IrradianceConvolution_irradianceVS.spirv IrradianceConvolution.hlsl > IrradianceConvolution_irradianceVS.log 2>&1
dxc.exe -T ps_6_0 -E irradiancePS -spirv -fspv-debug=vulkan-with-source -Fo IrradianceConvolution_irradiancePS.spirv IrradianceConvolution.hlsl > IrradianceConvolution_irradiancePS.log 2>&1

dxc.exe -T vs_6_0 -E prefilterVS -spirv -fspv-debug=vulkan-with-source -Fo PrefilteredEnvMap_prefilterVS.spirv PrefilteredEnvMap.hlsl > PrefilteredEnvMap_prefilterVS.log 2>&1
dxc.exe -T ps_6_0 -E prefilterPS -spirv -fspv-debug=vulkan-with-source -Fo PrefilteredEnvMap_prefilterPS.spirv PrefilteredEnvMap.hlsl > PrefilteredEnvMap_prefilterPS.log 2>&1

dxc.exe -T ps_6_0 -E baseColorDebugPS -fspv-debug=vulkan-with-source -spirv -Fo baseColorDebugPS.spirv SimpleShader.hlsl > baseColorDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E metalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo metalDebugPS.spirv SimpleShader.hlsl > metalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E roughDebugPS -fspv-debug=vulkan-with-source -spirv -Fo roughDebugPS.spirv SimpleShader.hlsl > roughDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E normalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo normalDebugPS.spirv SimpleShader.hlsl > normalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E vertNormalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo vertNormalDebugPS.spirv SimpleShader.hlsl > vertNormalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E uvDebugPS -fspv-debug=vulkan-with-source -spirv -Fo uvDebugPS.spirv SimpleShader.hlsl > uvDebugPS.log 2>&1