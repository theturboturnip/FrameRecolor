// DX12RealTimeRecolor.cpp : Defines the entry point for the application.
//

#include "DX12RealTimeRecolor.h"
#include <algorithm>
#include <memory>

using namespace RTR;

struct Arguments {
    LONG requestedWindowWidth;
    LONG requestedWindowHeight;
	bool enableWARP; // Windows Advanced Rasterization Platform - CPU supplementary rendering for older GPUs.
};
Arguments parse_command_line_args() {
    auto args = Arguments{
        .requestedWindowWidth = 1280,
        .requestedWindowHeight = 720,
        .enableWARP = false
    };

    // From https://www.3dgep.com/learning-directx-12-1
    int argc;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

    for (size_t i = 0; i < argc; ++i)
    {
        if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
        {
            args.requestedWindowWidth = ::wcstol(argv[++i], nullptr, 10);
        }
        if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
        {
            args.requestedWindowHeight = ::wcstol(argv[++i], nullptr, 10);
        }
        if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
        {
            args.enableWARP = true;
        }
    }

    // Free memory allocated by CommandLineToArgvW
    ::LocalFree(argv);

    return args;
}

extern "C" {
#include "libavformat/avformat.h"
}
struct ParsedFormat {
    /**
     * General type of the encoded data.
     */
    enum AVMediaType codec_type;
    /**
     * Specific type of the encoded data (the codec used).
     */
    enum AVCodecID   codec_id;
    /**
     * Additional information about the codec (corresponds to the AVI FOURCC).
     */
    uint32_t         codec_tag;

    enum AVPixelFormat pixel_format;

    int width, height;

    int64_t bitrate;

    /**
     * Video only. The aspect ratio (width / height) which a single pixel
     * should have when displayed.
     *
     * When the aspect ratio is unknown / undefined, the numerator should be
     * set to 0 (the denominator may have any value).
     */
    AVRational sample_aspect_ratio;

    /**
     * Video only. The order of the fields in interlaced video.
     */
    enum AVFieldOrder                  field_order;

    /**
     * Video only. Additional colorspace characteristics.
     */
    enum AVColorRange                  color_range;
    enum AVColorPrimaries              color_primaries;
    enum AVColorTransferCharacteristic color_trc;
    enum AVColorSpace                  color_space;
    enum AVChromaLocation              chroma_location;

    AVRational framerate;

    bool is_hdr() const {
        switch (color_trc) {
        case AVCOL_TRC_SMPTE2084:
            //case AVCOL_TRC_SMPTEST2084:
        case AVCOL_TRC_SMPTE428:
            //case AVCOL_TRC_SMPTEST428_1:
        case AVCOL_TRC_ARIB_STD_B67:
            return true;
        default:
            return false;
        }
    }
    bool may_be_interlaced() const {
        switch (field_order) {
        case AV_FIELD_PROGRESSIVE:
            return false;
        case AV_FIELD_UNKNOWN:
        default:
            return true;
        }
    }
};

ParsedFormat detect_format_of(const char* path) {
    // From https://stackoverflow.com/a/6452150/4248422

    // TODO handle non-wide paths using WideCharToMultiByte to convert a widechar path to utf-8 for libavformat to use??
    // https://stackoverflow.com/a/3999597/4248422
    AVFormatContext* pFormatCtx = avformat_alloc_context();
    assert(pFormatCtx);
    int err = avformat_open_input(&pFormatCtx, path, NULL, NULL);
    if (err) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        OutputDebugStringA( av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, err));
        abort();
    }
    err = avformat_find_stream_info(pFormatCtx, NULL);
    if (err) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        OutputDebugStringA(av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, err));
        abort();
    }
    
    int videoStream = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0) {
            assert(videoStream == -1 && "Multiple video streams!");
            videoStream = i;
        }
    }
    assert(videoStream != -1 && "No video streams!");

    auto pCodecCtx = pFormatCtx->streams[videoStream]->codecpar;
    assert(pCodecCtx != nullptr);

    auto format = ParsedFormat {
        .codec_type = pCodecCtx->codec_type,
        .codec_id = pCodecCtx->codec_id,
        .codec_tag = pCodecCtx->codec_tag,

        .pixel_format = (AVPixelFormat)pCodecCtx->format, // This is AVPixelFormat when for a video stream, otherwise AVSampleFormat

        .width = pCodecCtx->width,
        .height = pCodecCtx->height,

        .bitrate = pCodecCtx->bit_rate,

        .sample_aspect_ratio = pCodecCtx->sample_aspect_ratio,

        .field_order = pCodecCtx->field_order,

        .color_range = pCodecCtx->color_range,
        .color_primaries = pCodecCtx->color_primaries,
        .color_trc = pCodecCtx->color_trc,
        .color_space = pCodecCtx->color_space,
        .chroma_location = pCodecCtx->chroma_location,

        .framerate = pCodecCtx->framerate,
    };

    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);

    return format;
}

HRESULT dx12_profile_from_libavformat(ParsedFormat& format, GUID& outGUID) {
    switch (format.codec_id) {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
        outGUID = D3D12_VIDEO_DECODE_PROFILE_MPEG1_AND_MPEG2;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_MPEG2;
        break;
    case AV_CODEC_ID_MPEG4: // I think this is valid?
    case AV_CODEC_ID_H264:
        outGUID = D3D12_VIDEO_DECODE_PROFILE_H264;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_H264_STEREO_PROGRESSIVE;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_H264_STEREO;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_H264_MULTIVIEW;
        break;
    case AV_CODEC_ID_VC1:
        outGUID = D3D12_VIDEO_DECODE_PROFILE_VC1;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_VC1_D2010;
        break;

        //outGUID = D3D12_VIDEO_DECODE_PROFILE_MPEG4PT2_SIMPLE;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_MPEG4PT2_ADVSIMPLE_NOGMC;
    case AV_CODEC_ID_HEVC:
        if (format.is_hdr()) {
            outGUID = D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN10;
        }
        else {
            outGUID = D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN;
        }
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_VP9;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_VP9_10BIT_PROFILE2;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_VP8;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE0;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE1;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE2;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_AV1_12BIT_PROFILE2;
        //outGUID = D3D12_VIDEO_DECODE_PROFILE_AV1_12BIT_PROFILE2_420;
    default:
        return E_FAIL;
    }
    return S_OK;
}

// Global state
WindowState g_windowState;
bool g_windowInitialized = false;
DX12State g_dx12State;
bool g_dx12Initialized = false;

// Window callback function.
LRESULT CALLBACK window_message_callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_dx12Initialized) return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    switch (uMsg) {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;
    case WM_PAINT:
        g_dx12State.enqueueRenderAndPresentForNextFrame();
        break;
    }
    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

WindowState create_window(
    const wchar_t* windowClassName,
    HINSTANCE hInst,
    const wchar_t* windowTitle,
    LONG clientWidth,
    LONG clientHeight
) {
    assert(clientWidth > 0);
    assert(clientHeight > 0);

    // Register a window class for creating our render window with.
    WNDCLASSEXW windowClass = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW, // Always redraw the window if the width (HREDRAW) or height (VREDRAW) change
        .lpfnWndProc = &window_message_callback,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = hInst,
        .hIcon = ::LoadIcon(hInst, NULL),
        .hCursor = ::LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszMenuName = NULL,
        .lpszClassName = windowClassName,
        .hIconSm = ::LoadIcon(hInst, NULL)
    };

    static ATOM atom = ::RegisterClassExW(&windowClass);
    assert(atom > 0);

    // The class was successfully created, now create the window
    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    RECT windowRect = {
        .left=0,
        .top=0,
        .right=clientWidth,
        .bottom=clientHeight
    };
    // Add padding to the window size for window frame and other stuff that goes outside the "client area".
    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
    int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
    int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

    HWND hWnd = ::CreateWindowExW(
        NULL, // extended window style
        windowClassName,
        windowTitle,
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        NULL, // parent window
        NULL, // menu override
        hInst,
        nullptr
    );

    if (hWnd == NULL) {
        ExitOnWin32Error(TEXT("CreateWindowExW"));
    }

    ::GetWindowRect(hWnd, &windowRect);
    return WindowState{
        .hWnd = hWnd,
        .windowSizeWhenWindowed = windowRect,
        .clientWidth=(UINT)clientWidth,
        .clientHeight=(UINT)clientHeight,
    };
}

ComPtr<ID3D12CommandQueue> dx12_create_command_queue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

    return d3d12CommandQueue;
}

ComPtr<ID3D12DescriptorHeap> dx12_create_descriptor_heap(ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

DX12VideoDecodeState dx12_create_video_decoder(ComPtr<ID3D12VideoDevice> device, ParsedFormat& format) {
    auto decodeConfig = D3D12_VIDEO_DECODE_CONFIGURATION{
        .DecodeProfile = dx12_profile_from_libavformat(format),
        .BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE,
        .InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_FIELD_BASED // hedge bets if interlaced?
    };

    auto decoderDesc = D3D12_VIDEO_DECODER_DESC {
        .NodeMask = 0,
        .Configuration = decodeConfig
    };
    ComPtr<ID3D12VideoDecoder> decoder;
    ThrowIfFailed(device->CreateVideoDecoder(&decoderDesc, IID_PPV_ARGS(&decoder)));

    assert(format.width > 0);
    assert(format.width <= std::numeric_limits<u32>::max());
    assert(format.height > 0);
    assert(format.height <= std::numeric_limits<u32>::max());
    assert(format.bitrate > 0);
    assert(format.bitrate <= std::numeric_limits<u32>::max());

    auto decoderHeapDesc = D3D12_VIDEO_DECODER_HEAP_DESC{
        .NodeMask = 0,
        .Configuration = decodeConfig,
        .DecodeWidth = (u32)format.width,
        .DecodeHeight = (u32)format.height,
        //.Format = ,
        .FrameRate = DXGI_RATIONAL {
            .Numerator = (u32)format.framerate.num,
            .Denominator = (u32)format.framerate.den,
        },
        .BitRate = (u32)format.bitrate,
    };
    ComPtr<ID3D12VideoDecoderHeap> decoderHeap;
    ThrowIfFailed(device->CreateVideoDecoderHeap(&decoderHeapDesc, IID_PPV_ARGS(&decoder)));


}

void dx12_init(bool useWarp, ParsedFormat format480, ParsedFormat format2160) {
    assert(g_windowInitialized);

    // Start: make a DXGI adapter
    // Create a factory for making a DXGI adapter
    ComPtr<IDXGIFactory4> dxgiFactory4;
    // Enable debug messages in debug mode.
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    ComPtr<IDXGIAdapter4> dxgiAdapter4;
    if (useWarp) {
        // Ask specifically for a WARP adapter
        ComPtr<IDXGIAdapter1> dxgiAdapter1;
        ThrowIfFailed(dxgiFactory4->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    } else {
        // Go through the available adapters, get their descriptor, and if they're a hardware device capable of DX12 then shove them into dxgiAdapter4.
        // Keep going, re-shoving them into dxgiAdapter4 if they have more memory, so at the end we have the adapter with the maximum dedicated video memory.
        ComPtr<IDXGIAdapter1> dxgiAdapter1;
        SIZE_T maxDedicatedVideoMemory = 0;
        for (UINT i = 0; dxgiFactory4->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

            // Check to see if the adapter can create a D3D12 device without actually 
            // creating it. The adapter with the largest dedicated video memory
            // is favored.
            if (
                (dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory &&
                SUCCEEDED(D3D12CreateDevice(
                    dxgiAdapter1.Get(),
                    D3D_FEATURE_LEVEL_12_0,
                    __uuidof(ID3D12Device),
                    nullptr
                ))
            ) {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
            }
        }
    }

    // dxgiAdapter4 now has a valid value, let's create the device.
    // We don't do this in the EnumAdapters loop because we only want to create the device once.
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(dxgiAdapter4.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // This warning occurs when clearing render targets with a non-optimized value.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumCategories = 0;
        NewFilter.DenyList.pCategoryList = nullptr;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    // We have a device, create the command queue
    auto d3d12CommandQueue = dx12_create_command_queue(d3d12Device2, D3D12_COMMAND_LIST_TYPE_DIRECT);

    // We have a command queue and a window, create the swapchain.
    // First, look up if we're using a VRR display.
    bool swapchainTearingSupported = false;
    {
        // Attempt an upgrade from factory4 to factory5, then check feature support for ALLOW_TEARING.
        // CheckFeatureSupport writes directly to tearingSupported, but if it fails reset it to false just in case.
        ComPtr<IDXGIFactory5> dxgiFactory5;
        if (SUCCEEDED(dxgiFactory4.As(&dxgiFactory5))) {
            if (FAILED(dxgiFactory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &swapchainTearingSupported, sizeof(swapchainTearingSupported))))
            {
                swapchainTearingSupported = false;
            }
        }
    }
    // Then create the descriptor for the swapchain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
        .Width = g_windowState.clientWidth,
        .Height = g_windowState.clientHeight,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = { 1, 0 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = NUM_INFLIGHT_FRAMES,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        // It is recommended to always allow tearing if tearing support is available.
        .Flags = swapchainTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
    };
    // Then actually create the swapchain. I don't know why the tutorial makes SwapChain1 then upgrades, instead of asking for swapchain 4.
    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    {
        ComPtr<IDXGISwapChain1> dxgiSwapChain1;
        ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
            d3d12CommandQueue.Get(),
            g_windowState.hWnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &dxgiSwapChain1
        ));

        // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
        // will be handled manually.
        ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(g_windowState.hWnd, DXGI_MWA_NO_ALT_ENTER));

        ThrowIfFailed(dxgiSwapChain1.As(&dxgiSwapChain4));
    }

    // We have the device, create the descriptor heap and backbuffer array
    auto renderTargetDescriptorHeap = dx12_create_descriptor_heap(d3d12Device2, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_INFLIGHT_FRAMES);
    UINT renderTargetDescriptorSize = d3d12Device2->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    std::array<ComPtr<ID3D12Resource>, NUM_INFLIGHT_FRAMES> backBuffers;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(renderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    for (u32 i = 0; i < NUM_INFLIGHT_FRAMES; i++) {
        ThrowIfFailed(dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])));
        d3d12Device2->CreateRenderTargetView(
            backBuffers[i].Get(),
            nullptr,
            rtvHandle
        );
        rtvHandle.Offset(renderTargetDescriptorSize);
    }

    // We have the device, create the fence
    u64 inflightFrameFenceValue = 0;
    u64 nextFrameFenceValue = 1;
    ComPtr<ID3D12Fence> inflightFrameFence;
    ThrowIfFailed(d3d12Device2->CreateFence(inflightFrameFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&inflightFrameFence)));
    // Also create the windows event handle for waiting on the fence to change
    HANDLE inflightFrameFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (inflightFrameFenceEvent == NULL) {
        ExitOnWin32Error(TEXT("CreateEvent"));
    }

    std::array<DX12InflightFrameState, NUM_INFLIGHT_FRAMES> perFrameState;
    for (u32 i = 0; i < NUM_INFLIGHT_FRAMES; i++) {
        // Setup the command allocator
        auto command_list_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(d3d12Device2->CreateCommandAllocator(command_list_type, IID_PPV_ARGS(&perFrameState[i].commandAllocator)));
        // Setup the command list
        ThrowIfFailed(d3d12Device2->CreateCommandList(
            0,
            command_list_type,
            perFrameState[i].commandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&perFrameState[i].commandList)
        ));
        // Close the list so the next thing we do (which will be at the start of the record phase) can be Reset()
        ThrowIfFailed(perFrameState[i].commandList->Close());

        perFrameState[i].lastFrameFence = 0;
        perFrameState[i].backBufferIdx = 0;
    }

    // Get video decode capabilities
    ComPtr<ID3D12VideoDevice> videoDevice;
    ThrowIfFailed(d3d12Device2.As(&videoDevice));

    GUID format480GUID;
    ThrowIfFailed(dx12_profile_from_libavformat(format480, format480GUID));
    ComPtr<ID3D12VideoDecoder> videoDecoder480;
    auto videoDecoder480Desc = D3D12_VIDEO_DECODER_DESC{
        .NodeMask = 0,
        .Configuration = D3D12_VIDEO_DECODE_CONFIGURATION {
            .DecodeProfile = format480GUID,
            .BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE,
            .InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_FIELD_BASED // hedge bets if interlaced?
        }
    };
    ThrowIfFailed(videoDevice->CreateVideoDecoder(&videoDecoder480Desc, IID_PPV_ARGS(&videoDecoder480)));

    GUID format2160GUID;
    ThrowIfFailed(dx12_profile_from_libavformat(format2160, format2160GUID));
    ComPtr<ID3D12VideoDecoder> videoDecoder2160;
    auto videoDecoder2160Desc = D3D12_VIDEO_DECODER_DESC{
        .NodeMask = 0,
        .Configuration = D3D12_VIDEO_DECODE_CONFIGURATION {
            .DecodeProfile = format2160GUID,
            .BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE,
            .InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_FIELD_BASED // hedge bets if interlaced?
        }
    };
    ThrowIfFailed(videoDevice->CreateVideoDecoder(&videoDecoder2160Desc, IID_PPV_ARGS(&videoDecoder2160)));

    g_dx12State = DX12State{
        .device = d3d12Device2,
        .videoDevice = videoDevice,
        .commandQueue = d3d12CommandQueue,
        
        .swapchain = dxgiSwapChain4,
        .swapchainVsync = true,
        .swapchainTearingSupported = swapchainTearingSupported,
        .swapchainFullscreen = false,

        .renderTargetDescriptorHeap=renderTargetDescriptorHeap,
        .renderTargetDescriptorSize=renderTargetDescriptorSize,

        .backBuffers=backBuffers,
        .perFrameState = perFrameState,

        .inflightFrameFence=inflightFrameFence,
        .inflightFrameFenceValue=inflightFrameFenceValue,
        .inflightFrameFenceEvent=inflightFrameFenceEvent,
    };
}

void DX12State::enqueueRenderAndPresentForNextFrame() {
    auto frameIndex = swapchain->GetCurrentBackBufferIndex();

    auto backBuffer = backBuffers[frameIndex];
    auto commandAllocator = perFrameState[frameIndex].commandAllocator;
    auto commandList = perFrameState[frameIndex].commandList;

    // Wait for this in-flight frame to finish, so we can use the resources
    cpuWaitForFenceToHaveAtLeastValue(perFrameState[frameIndex].lastFrameFence);
    // Reset the command allocator and list(?)
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), nullptr);
    
    // Enqueue a command to transition the backBuffer from (presented) to (render target)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        commandList->ResourceBarrier(1, &barrier);
    }
    // Enqueue a command to clear the freshly-transitioned backBuffer
    {
        FLOAT clearColor[] = { 0.4f + (0.0006f * (inflightFrameFenceValue % 1000)), 0.6f, 0.9f, 1.0f};
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
            renderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            frameIndex,
            renderTargetDescriptorSize
        );

        commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }
    // Enqueue a command to transition the backBuffer from (render target) to (presentable)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        commandList->ResourceBarrier(1, &barrier);
    }

    // That's all we can do with a command list. Enqueue those commands.
    ThrowIfFailed(commandList->Close());

    ID3D12CommandList* const commandLists[] = {
        commandList.Get()
    };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Enqueue a command to present the freshly presentable backBuffer.
    // We don't need to present extra information about the backBuffer
    // because the swapchain is the one who told us which resource it would pull from next.
    // It's the swapchain's world, we just live in it.
    {
        UINT syncInterval = swapchainVsync ? 1 : 0;
        UINT presentFlags = (swapchainTearingSupported && !swapchainVsync) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(swapchain->Present(syncInterval, presentFlags));
    }

    // Now we've submitted all the work, enqueue a request to increment the fence once the work is finished.
    perFrameState[frameIndex].lastFrameFence = incrementFenceFromGPUQueue();
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    auto args = parse_command_line_args();

    auto format480 = detect_format_of("../../../../480p.mp4");
    auto format2160 = detect_format_of("../../../../2160p.mkv");

    OutputDebugStringA(avcodec_get_name(format480.codec));
    OutputDebugStringA(avcodec_get_name(format2160.codec));
    //fprintf(stderr, "480p:  %s\n2160p: %s\n", avcodec_get_name(format480), avcodec_get_name(format2160));

#if defined(_DEBUG)
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    g_windowState = create_window(
        L"DX12WindowClass",
        hInstance,
        L"Real Time Recolor",
        args.requestedWindowWidth,
        args.requestedWindowHeight
    );
    g_windowInitialized = true;
    dx12_init(args.enableWARP, format480, format2160);
    g_dx12Initialized = true;

    ::ShowWindow(g_windowState.hWnd, SW_SHOW);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        else {
            g_dx12State.enqueueRenderAndPresentForNextFrame();
        }
    }

    // Make sure the command queue has finished all commands before closing.
    g_dx12State.flush();

    ::CloseHandle(g_dx12State.inflightFrameFenceEvent);

	return 0;
}
