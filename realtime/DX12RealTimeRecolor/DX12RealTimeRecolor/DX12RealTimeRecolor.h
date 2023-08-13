// DX12RealTimeRecolor.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "Utils/windxheaders.h"

#include <array>
#include <chrono>

namespace RTR {
    using u32 = uint32_t;
    using u64 = uint64_t;

    constexpr u32 NUM_INFLIGHT_FRAMES = 3;

    struct WindowState {
        HWND hWnd;
        // Used to remember what the width/height was before we went into fullscreen mode.
        RECT windowSizeWhenWindowed;
        UINT clientWidth;
        UINT clientHeight;
    };

    struct DX12InflightFrameState {
        // Tracking for "is this frame in-flight?"
        // If lastFrameFence <= inflightFrameFence.GetCompletedValue() these resources are ready to use.
        u64 lastFrameFence;
        UINT backBufferIdx;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12GraphicsCommandList> commandList; // TODO the tutorial creates only one of these... but they're tied to the commandAllocator!
    };

    struct DX12State {
        ComPtr<ID3D12Device2> device;
        ComPtr<ID3D12CommandQueue> commandQueue;

        // Swapchain and associated configuration
        ComPtr<IDXGISwapChain4> swapchain;
        bool swapchainVsync;
        bool swapchainTearingSupported; // "tearing supported" = running on a VRR display
        bool swapchainFullscreen;

        // The heap for storing render target descriptors.
        // The size of a render target descriptor is vendor-dependent.
        ComPtr<ID3D12DescriptorHeap> renderTargetDescriptorHeap;
        UINT renderTargetDescriptorSize;

        std::array<ComPtr<ID3D12Resource>, NUM_INFLIGHT_FRAMES> backBuffers;
        std::array<DX12InflightFrameState, NUM_INFLIGHT_FRAMES> perFrameState;

        // A fence holds a 64-bit unsigned value that the GPU increments.
        // More precisely, we put commands in the GPU queue to set the fence to specific values.
        ComPtr<ID3D12Fence> inflightFrameFence;
        // The latest value we've enqueued on the GPU to set the fence to.
        u64 inflightFrameFenceValue;
        HANDLE inflightFrameFenceEvent;

        u64 incrementFenceFromGPUQueue() {
            u64 fenceValueToRequest = (inflightFrameFenceValue++);
            ThrowIfFailed(commandQueue->Signal(inflightFrameFence.Get(), fenceValueToRequest));
            return fenceValueToRequest;
        }
        void cpuWaitForFenceToHaveAtLeastValue(u64 expectedValue,
            std::chrono::milliseconds duration = std::chrono::milliseconds::max()) {
            if (inflightFrameFence->GetCompletedValue() < expectedValue)
            {
                ThrowIfFailed(inflightFrameFence->SetEventOnCompletion(expectedValue, inflightFrameFenceEvent));
                ::WaitForSingleObject(inflightFrameFenceEvent, static_cast<DWORD>(duration.count()));
            }
        }

        void enqueueRenderAndPresentForNextFrame();
        void flush() {
            u64 valueOfFenceWhenAllWorkIsFinished = incrementFenceFromGPUQueue();
            cpuWaitForFenceToHaveAtLeastValue(valueOfFenceWhenAllWorkIsFinished);
        }
    };
}