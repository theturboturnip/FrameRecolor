// DX11RealTimeRecolor.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "Utils/windxheaders.h"

#include <array>
#include <chrono>

namespace RTR {
    using u32 = uint32_t;
    using u64 = uint64_t;

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

        // TODO multiple backbuffers
        ComPtr<ID3D11RenderTargetView> backBuffer;
        ComPtr<ID3D11Texture2D> backBufferTex;

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

        ComPtr<ID3D11Texture2D> latestFrame = nullptr;

        void readFrame();

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