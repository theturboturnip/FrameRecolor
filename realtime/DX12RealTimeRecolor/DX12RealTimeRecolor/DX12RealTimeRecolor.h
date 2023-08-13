// DX12RealTimeRecolor.h : Include file for standard system include files,
// or project specific include files.

#pragma once

// Based on https://www.3dgep.com/learning-directx-12-1/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <strsafe.h> // For StringCchPrintf
#include <shellapi.h> // For CommandLineToArgvW

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
// SAMUEL NOTE - bad practice to make functions that overlap with windows macros anyway...
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
// SAMUEL NOTE - The tutorial did `using namespace Microsoft::WRL;` but I prefer to only include the specific classes I need.
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
// SAMUEL NOTE - Don't want to bother with installing this from https://github.com/microsoft/DirectX-Headers right now.
//#include <d3dx12.h>

#include <exception>
// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}
// From https://learn.microsoft.com/en-us/windows/win32/debug/retrieving-the-last-error-code
void ExitOnWin32Error(LPCTSTR lpszFunction)
{
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"),
        lpszFunction, dw, lpMsgBuf);
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(dw);
}

namespace RTR {
    using u32 = uint32_t;
    using u64 = uint64_t;

    constexpr u32 NUM_INFLIGHT_FRAMES = 3;

    struct WindowState {
        HWND hWnd;
        // Used to remember what the width/height was before we went into fullscreen mode.
        RECT windowSizeWhenWindowed;
    };

    struct DX12InflightFrameState {
        ComPtr<ID3D12Resource> backBuffer;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        // Tracking for "is this frame in-flight?"
        // If inflight is set, this frame has been passed to the GPU for rendering and we can't use the backbuffer or commandallocator on the CPU.
        // valueForFrameFence is the value the inflightFrameFence will take when this frame is complete, setting inflight to false.
        // valueForFrameFence only has meaning when inflight=true.
        bool inflight;
        u64 valueForFrameFence;
    };

    struct DX12State {
        ComPtr<ID3D12Device2> device;
        ComPtr<ID3D12CommandQueue> commandQueue;

        // Swapchain and associated configuration
        ComPtr<IDXGISwapChain4> swapchain;
        bool swapchainVsync;
        bool swapchainTearingSupported; // ?
        bool swapchainFullscreen;

        // The heap for storing render target descriptors.
        // The size of a render target descriptor is vendor-dependent.
        ComPtr<ID3D12DescriptorHeap> renderTargetDescriptorHeap;
        UINT renderTargetDescriptorSize;

        ComPtr<ID3D12CommandList> commandList; // Used for recording commands, we only ever need one at a time

        ComPtr<ID3D12Fence> inflightFrameFence;
        u64 inflightFrameFenceValue;
        HANDLE inflightFrameFenceEvent;

        DX12InflightFrameState perFrameState[NUM_INFLIGHT_FRAMES];
        UINT currentInflightFrame;
    };
}