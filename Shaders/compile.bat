dxc.exe -T vs_6_0 -E simpleVS -spirv -Fo SimpleShader_simpleVS.spirv SimpleShader.hlsl > SimpleShader_simpleVS.log 2>&1
dxc.exe -T ps_6_0 -E simplePS -spirv -Fo SimpleShader_simplePS.spirv SimpleShader.hlsl > SimpleShader_simplePS.log 2>&1

dxc.exe -T ps_6_0 -E baseColorDebugPS -spirv -Fo baseColorDebugPS.spirv SimpleShader.hlsl > baseColorDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E metalDebugPS -spirv -Fo metalDebugPS.spirv SimpleShader.hlsl > metalDebugPS.log 2>&1
dxc.exe -T ps_6_0 -E roughDebugPS -spirv -Fo roughDebugPS.spirv SimpleShader.hlsl > roughDebugPS.log 2>&1