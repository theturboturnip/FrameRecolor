﻿// DX11RealTimeRecolor.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "Utils/windxheaders.h"

#include <array>
#include <chrono>

namespace RTR {
    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    constexpr u32 NUM_INFLIGHT_FRAMES = 2;

    constexpr struct {
        float x, y, z, w;
        float u, v;
    } vertex_buffer[4] = {
        { .x = -1, .y = -1, .z = 0, .w = 0, .u = 0, .v = 0 },
        { .x = -1, .y = 1, .z = 0, .w = 0, .u = 0, .v = 1 },
        { .x = 1, .y = -1, .z = 0, .w = 0, .u = 1, .v = 0 },
        { .x = 1, .y = 1, .z = 0, .w = 0, .u = 1, .v = 1 },
    };
    constexpr u16 index_buffer[6] = {
        0, 1, 2,
        2, 1, 3
    };

    struct WindowState {
        HWND hWnd;
        // Used to remember what the width/height was before we went into fullscreen mode.
        RECT windowSizeWhenWindowed;
        UINT clientWidth;
        UINT clientHeight;
    };

    struct DX11State {
        ComPtr<IDXGISwapChain> swapchain;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> deviceContext;

        // We don't need multiple back buffers - DX11 automagically switches the backing of the RTV
        ComPtr<ID3D11RenderTargetView> backBuffer;
        //ComPtr<ID3D11Texture2D> backBufferTex;

        ComPtr<ID3D11VertexShader> posuv_vert;
        ComPtr<ID3D11InputLayout> posuv_inputlayout;
        ComPtr<ID3D11PixelShader> yuv2RGB_frag;

        ComPtr<ID3D11Buffer> quadVertexBuffer;
        ComPtr<ID3D11Buffer> quadIndexBuffer;

        D3D11_VIEWPORT viewport;

        void enqueueRenderAndPresentForNextFrame(ComPtr<ID3D11Texture2D> frame);
        void flushAndClose();
    };

    struct FFMpegPerVideoState {
        AVFormatContext* input_ctx = nullptr;
        AVCodec* decoder = nullptr;
        AVCodecContext* decoder_ctx = nullptr;
        int video_stream_index = 0;
        AVStream* video_stream = nullptr;
        AVBufferRef* hw_device_ctx = nullptr;
        AVPacket* packet = nullptr;
        AVFrame* frame = nullptr;

        ComPtr<ID3D11Texture2D> lastFrameCopyTarget;

        void readFrame(DX11State& dx11State);

        void flushAndClose() {
            // If the decoder is still around, flush it
            if (decoder_ctx) {
                packet->data = NULL;
                packet->size = 0;
                avcodec_send_packet(decoder_ctx, packet);
            }

            if (packet) {
                av_packet_unref(packet);
                av_packet_free(&packet); // nulls it out
            }
            if (hw_device_ctx) {
                av_buffer_unref(&hw_device_ctx); // nulls it out
            }
            // video_stream freed by freeing decoder ctx
            video_stream = nullptr;
            if (decoder_ctx) {
                avcodec_free_context(&decoder_ctx); // nulls it out
            }
            decoder = nullptr; // I think *decoder is allocated statically? or by the input_ctx?
            if (input_ctx) {
                avformat_free_context(input_ctx);
                input_ctx = nullptr;
            }
        }
    };

    struct FFMpegState {
        AVHWDeviceType deviceType;
        AVPixelFormat dx11PixelFormat;
    };
}