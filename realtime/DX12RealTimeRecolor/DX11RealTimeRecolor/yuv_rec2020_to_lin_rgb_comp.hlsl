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
RWTexture2D<float4> rgbDst : t2;

cbuffer CONSTANTS: register(b0) {
	uint2 texDims;
}

#include "includes.hlsl"

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
	if (all(DTid.xy < texDims)) {
		uint y = ySource.Load(uint4(DTid.x, DTid.y, 0, 0)) >> 6;
		uint2 uv = uvSource.Load(uint4(DTid.x / 2, DTid.y / 2, 0, 0)) >> 6;

		float3 linear_rgb = yuv_rec2020_10bit_to_linear_rgb(y, uv);

		rgbDst[DTid.xy] = float4(linear_rgb, 1.0);
		//rgbDst[DTid.xy] = float4(y, uv.x, uv.y, 1.0);
	}

}
