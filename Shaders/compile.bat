dxc.exe -T vs_6_0 -E simpleVS -spirv -Fo SimpleShader_simpleVS.spirv SimpleShader.hlsl
dxc.exe -T ps_6_0 -E simplePS -spirv -Fo SimpleShader_simplePS.spirv SimpleShader.hlsl