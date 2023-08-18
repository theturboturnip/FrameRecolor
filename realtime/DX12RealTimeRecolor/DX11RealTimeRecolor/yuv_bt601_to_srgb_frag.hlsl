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

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos         : SV_POSITION;
	float2 texCoord    : TEXCOORD0;
};

Texture2D<float>  luminanceChannel   : t0;
Texture2D<float2> chrominanceChannel : t1;
SamplerState      defaultSampler {
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
};

#include "includes.hlsl"

min16float4 main(PixelShaderInput input) : SV_TARGET
{
	float y = luminanceChannel.Sample(defaultSampler, input.texCoord);
	float2 uv = chrominanceChannel.Sample(defaultSampler, input.texCoord);

	return min16float4(yuv_bt601_to_srgb(float3(y, uv)), 1.f);
}
