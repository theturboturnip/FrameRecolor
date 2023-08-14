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

        void enqueueRenderAndPresentForNextFrame();
        void flushAndClose();
    };
}