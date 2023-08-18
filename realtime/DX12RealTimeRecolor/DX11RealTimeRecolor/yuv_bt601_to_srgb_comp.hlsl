// Copied from https://raw.githubusercontent.com/balapradeepswork/D3D11NV12Rendering/master/D3D11NV12Rendering/PixelShader.hlsl
// which included the following disclaimer:

//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

RWTexture2DArray<uint> ySource: t0;
RWTexture2DArray<uint2> uvSource: t1;
RWTexture2D<float4> srgb : t2;

cbuffer CONSTANTS: register(b0) {
	uint2 texDims;
}

#include "includes.hlsl"

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
	if (all(DTid.xy < texDims)) {
		float y = ySource.Load(uint4(DTid.x, DTid.y, 0, 0)) / 255.0;
		float2 uv = uvSource.Load(uint4(DTid.x / 2, DTid.y / 2, 0, 0)) / 255.0;
		srgb[DTid.xy] = float4(yuv_bt601_to_srgb(float3(y, uv.x, uv.y)), 1.f);
	}
}
