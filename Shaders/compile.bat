dxc.exe -T vs_6_0 -E simpleVS -spirv -fspv-debug=vulkan-with-source -Fo SimpleShader_simpleVS.spirv SimpleShader.hlsl > SimpleShader_simpleVS.log 2>&1
dxc.exe -T ps_6_0 -E simplePS -spirv -fspv-debug=vulkan-with-source -Fo SimpleShader_simplePS.spirv SimpleShader.hlsl > SimpleShader_simplePS.log 2>&1

dxc.exe -T vs_6_0 -E screenSpaceVS -spirv -fspv-debug=vulkan-with-source -Fo ScreenSpace_screenSpaceVS.spirv ScreenSpace.hlsl > ScreenSpace_screenSpaceVS.log 2>&1
dxc.exe -T ps_6_0 -E toneMapPS -spirv -fspv-debug=vulkan-with-source -Fo ScreenSpace_toneMapPS.spirv ScreenSpace.hlsl > ScreenSpace_toneMapPS.log 2>&1
dxc.exe -T ps_6_0 -E passThroughPS -spirv -fspv-debug=vulkan-with-source -Fo ScreenSpace_passThroughPS.spirv ScreenSpace.hlsl > ScreenSpace_passThroughPS.log 2>&1

dxc.exe -T ps_6_0 -E baseColorDebugPS -fspv-debug=vulkan-with-source -spirv -Fo baseColorDebugPS.spirv SimpleShader.hlsl > baseColorDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E metalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo metalDebugPS.spirv SimpleShader.hlsl > metalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E roughDebugPS -fspv-debug=vulkan-with-source -spirv -Fo roughDebugPS.spirv SimpleShader.hlsl > roughDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E normalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo normalDebugPS.spirv SimpleShader.hlsl > normalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E vertNormalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo vertNormalDebugPS.spirv SimpleShader.hlsl > vertNormalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E uvDebugPS -fspv-debug=vulkan-with-source -spirv -Fo uvDebugPS.spirv SimpleShader.hlsl > uvDebugPS.log 2>&1