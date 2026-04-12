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

dxc.exe -T vs_6_0 -E prefilterVS -spirv -fspv-debug=vulkan-with-source -Fo PrefilteredEnvironment_prefilterVS.spirv PrefilteredEnvironment.hlsl > PrefilteredEnvironment_prefilterVS.log 2>&1
dxc.exe -T ps_6_0 -E prefilterPS -spirv -fspv-debug=vulkan-with-source -Fo PrefilteredEnvironment_prefilterPS.spirv PrefilteredEnvironment.hlsl > PrefilteredEnvironment_prefilterPS.log 2>&1

dxc.exe -T vs_6_0 -E integrateBRDF_VS -spirv -fspv-debug=vulkan-with-source -Fo IntegrateBRDF_integrateBRDF_VS.spirv IntegrateBRDF.hlsl > IntegrateBRDF_integrateBRDF_VS.log 2>&1
dxc.exe -T ps_6_0 -E integrateBRDF_PS -spirv -fspv-debug=vulkan-with-source -Fo IntegrateBRDF_integrateBRDF_PS.spirv IntegrateBRDF.hlsl > IntegrateBRDF_integrateBRDF_PS.log 2>&1

dxc.exe -T vs_6_0 -E shadowVS -spirv -fspv-debug=vulkan-with-source -Fo Shadow_shadowVS.spirv Shadow.hlsl > Shadow_shadowVS.log 2>&1
dxc.exe -T ps_6_0 -E shadowPS -spirv -fspv-debug=vulkan-with-source -Fo Shadow_shadowPS.spirv Shadow.hlsl > Shadow_shadowPS.log 2>&1

dxc.exe -T ps_6_0 -E baseColorDebugPS -fspv-debug=vulkan-with-source -spirv -Fo baseColorDebugPS.spirv SimpleShader.hlsl > baseColorDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E metalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo metalDebugPS.spirv SimpleShader.hlsl > metalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E roughDebugPS -fspv-debug=vulkan-with-source -spirv -Fo roughDebugPS.spirv SimpleShader.hlsl > roughDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E normalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo normalDebugPS.spirv SimpleShader.hlsl > normalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E vertNormalDebugPS -fspv-debug=vulkan-with-source -spirv -Fo vertNormalDebugPS.spirv SimpleShader.hlsl > vertNormalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E uvDebugPS -fspv-debug=vulkan-with-source -spirv -Fo uvDebugPS.spirv SimpleShader.hlsl > uvDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E linearViewDepthDebugPS -fspv-debug=vulkan-with-source -spirv -Fo linearViewDepthDebugPS.spirv SimpleShader.hlsl > linearViewDepthDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E shadowCascadeDebugPS -fspv-debug=vulkan-with-source -spirv -Fo shadowCascadeDebugPS.spirv SimpleShader.hlsl > shadowCascadeDebugPS.log 2>&1