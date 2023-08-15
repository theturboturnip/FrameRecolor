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

RWTexture2D<float>  luminanceChannel;
RWTexture2D<float2> chrominanceChannel;

RWTexture2D<float4> rgb;

cbuffer CONSTANTS: register(b0) {
	uint2 texLumDims;
	uint2 texChromDims;
}

// Derived from https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
// Section: Converting 8-bit YUV to RGB888
static const float3x3 YUVtoRGBCoeffMatrix =
{
	1.164383f,  1.164383f, 1.164383f,
	0.000000f, -0.391762f, 2.017232f,
	1.596027f, -0.812968f, 0.000000f
};

float3 ConvertYUVtoRGB(float3 yuv)
{
	// Derived from https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
	// Section: Converting 8-bit YUV to RGB888

	// These values are calculated from (16 / 255) and (128 / 255)
	yuv -= float3(0.062745f, 0.501960f, 0.501960f);
	yuv = mul(yuv, YUVtoRGBCoeffMatrix);

	return saturate(yuv);
}

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
	if (all(DTid.xy < texLumDims) && all((DTid.xy / 2) < texChromDims)) {
		float y = luminanceChannel.Load(DTid.xy);
		float2 uv = chrominanceChannel.Load(DTid.xy / 2);

		rgb[DTid.xy] = float4(ConvertYUVtoRGB(float3(y, uv)), 1.f);
	}
}
