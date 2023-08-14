// DX11RealTimeRecolor.cpp : Defines the entry point for the application.
//

#include "DX11RealTimeRecolor.h"
using namespace RTR;

struct Arguments {
    LONG requestedWindowWidth;
    LONG requestedWindowHeight;
};
Arguments parse_command_line_args() {
    auto args = Arguments{
        .requestedWindowWidth = 1280,
        .requestedWindowHeight = 720,
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
    }

    // Free memory allocated by CommandLineToArgvW
    ::LocalFree(argv);

    return args;
}

// Global state
WindowState g_windowState;
bool g_windowInitialized = false;
bool g_dx11Initialized = false;

// Window callback function.
LRESULT CALLBACK window_message_callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_dx11Initialized) return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    switch (uMsg) {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;
    case WM_PAINT:
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
        .left = 0,
        .top = 0,
        .right = clientWidth,
        .bottom = clientHeight
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
        .clientWidth = (UINT)clientWidth,
        .clientHeight = (UINT)clientHeight,
    };
}

DX11State dx11_init() {
    assert(g_windowInitialized);

    /*DXGI_MODE_DESC BufferDesc;
    DXGI_SAMPLE_DESC SampleDesc;
    DXGI_USAGE BufferUsage;
    UINT BufferCount;
    HWND OutputWindow;
    BOOL Windowed;
    DXGI_SWAP_EFFECT SwapEffect;
    UINT Flags;*/

    auto swapchainDesc = DXGI_SWAP_CHAIN_DESC{
        .BufferDesc = DXGI_MODE_DESC {
            .Width = g_windowState.clientWidth,
            .Height = g_windowState.clientHeight,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM
        },
        .SampleDesc = DXGI_SAMPLE_DESC {
            .Count = 1,
            .Quality = 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 1,
        .OutputWindow = g_windowState.hWnd,
        .Windowed = true,
    };

    ComPtr<IDXGISwapChain> swapchain = nullptr;
    ComPtr<ID3D11Device> device = nullptr;
    ComPtr<ID3D11DeviceContext> deviceContext = nullptr;

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0
    };

    ThrowIfFailed(D3D11CreateDeviceAndSwapChain(
        NULL, // DXGI decides on the adapter
        D3D_DRIVER_TYPE_HARDWARE, // Use dedicated hardware
        NULL, // The software module, only used with D3D_DRIVER_TYPE_SOFTWARE
#if defined(_DEBUG)
        D3D11_CREATE_DEVICE_DEBUG, // Flags, enable debug
#else
        0, // Flags, no debug
#endif
        featureLevels, ARRAYSIZE(featureLevels), // requested feature levels + count
        D3D11_SDK_VERSION, // For forwards compat - a later driver will notice what SDK we compiled for and make sure we get what we expect
        &swapchainDesc,
        &swapchain,
        &device,
        NULL,
        &deviceContext
    ));

    ComPtr<ID3D11Texture2D> backBufferTex;
    ComPtr<ID3D11RenderTargetView> backBuffer;
    ThrowIfFailed(swapchain->GetBuffer(0, IID_PPV_ARGS(&backBufferTex)));
    ThrowIfFailed(device->CreateRenderTargetView(backBufferTex.Get(), NULL, &backBuffer));

    return DX11State{
        .swapchain = swapchain,
        .device = device,
        .deviceContext = deviceContext,

        .backBuffer = backBuffer,
        .backBufferTex = backBufferTex,

        .viewport = {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width = g_windowState.clientWidth * 1.0f,
            .Height = g_windowState.clientHeight * 1.0f,
        }
    };
}

void DX11State::enqueueRenderAndPresentForNextFrame(ComPtr<ID3D11Texture2D> frame) {
    ID3D11RenderTargetView* targets[] = { backBuffer.Get() };
    deviceContext->OMSetRenderTargets(1, targets, NULL);
    deviceContext->RSSetViewports(1, &viewport);

    // clear the back buffer
    FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
    deviceContext->ClearRenderTargetView(backBuffer.Get(), clearColor);

    // copy the frame in
    if (frame) {
        //deviceContext->CopySubresourceRegion(
        //    backBufferTex.Get(), 0, 0, 0, 0,
        //    frame.Get(), 0, nullptr);
    }

    // switch the back buffer and the front buffer
    swapchain->Present(0, 0);
}
void DX11State::flushAndClose() {
    deviceContext.Reset();
    device.Reset();
    swapchain.Reset();
}

// Callback used by 
AVPixelFormat get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts) {
    for (auto p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_D3D11)
            return AV_PIX_FMT_D3D11;
    }
    fprintf(stderr, "Failed to get HW surface format\n");
    return AV_PIX_FMT_NONE;
}

FFMpegPerVideoState ffmpeg_create_decoder(DX11State& dx11State, const char* path) {
    FFMpegPerVideoState state = {};

    // Open the video and figure out what streams it has
    ThrowIfFfmpegFail(avformat_open_input(&state.input_ctx, path, NULL, NULL));
    ThrowIfFfmpegFail(avformat_find_stream_info(state.input_ctx, NULL));

    // Find the best video stream, allocating and filling in certain properties of a decoder (but not opening the decoder yet...)
    // The function specification for this claims to write sometihng into const AVCodec**... breaks const correctness...
    state.video_stream_index = ThrowIfFfmpegFail(av_find_best_stream(state.input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, (const AVCodec**) &state.decoder, 0));
    state.video_stream = state.input_ctx->streams[state.video_stream_index];

    // Allocate a decoder context, fill it with parameters for the video codec we want and the hardware device context...
    state.decoder_ctx = avcodec_alloc_context3(state.decoder);
    assert(state.decoder_ctx);
    // Set up the decoder with the correct parameters for this video stream's codec
    ThrowIfFfmpegFail(avcodec_parameters_to_context(state.decoder_ctx, state.video_stream->codecpar));
    // Request the D3D11 format
    state.decoder_ctx->get_format = get_hw_format;
    // ... I think this means there's only one DX11 frame texture, and it gets reused?
    av_opt_set_int(state.decoder_ctx, "refcounted_frames", 1, 0);
    // Create a hardware device context using the DX11 device, put it into the decoder_ctx
    {
        AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        AVHWDeviceContext* device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
        AVD3D11VADeviceContext* d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
        d3d11va_device_ctx->device = dx11State.device.Get();
        state.decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_hwdevice_ctx_init(state.decoder_ctx->hw_device_ctx);
    }

    // Decoder context now has all parameters filled in, now actually open the decoder
    ThrowIfFfmpegFail(avcodec_open2(state.decoder_ctx, state.decoder, NULL));

    state.packet = av_packet_alloc();
    state.frame = av_frame_alloc();

    return state;
}

void FFMpegPerVideoState::readFrame() {
    do {
        av_packet_unref(packet);
        ThrowIfFfmpegFail(av_read_frame(input_ctx, packet));
    } while (packet->stream_index != video_stream_index);

    ThrowIfFfmpegFail(avcodec_send_packet(decoder_ctx, packet));
    
    int ret = avcodec_receive_frame(decoder_ctx, frame);
    switch (ret) {
    case 0:
        this->latestFrame = (ID3D11Texture2D*)frame->data[0];
        break;
    case AVERROR_EOF:
    case AVERROR(EAGAIN):
        this->latestFrame = nullptr; // Don't copy more stuff around, hopefully frame is still valid with the last frame of the video...
        break;
    case AVERROR(EINVAL):
        ThrowIfFfmpegFail(ret);
    default:
        ThrowIfFfmpegFail(ret);
    }
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    auto args = parse_command_line_args();

    //auto format480 = detect_format_of("../../../../480p.mp4");
    //auto format2160 = detect_format_of("../../../../2160p.mkv");

    //OutputDebugStringA(avcodec_get_name(format480.codec));
    //OutputDebugStringA(avcodec_get_name(format2160.codec));
    //fprintf(stderr, "480p:  %s\n2160p: %s\n", avcodec_get_name(format480), avcodec_get_name(format2160));

    g_windowState = create_window(
        L"DX12WindowClass",
        hInstance,
        L"Real Time Recolor",
        args.requestedWindowWidth,
        args.requestedWindowHeight
    );
    g_windowInitialized = true;

    {
        DX11State dx11State = dx11_init();
        FFMpegPerVideoState ffmpeg480 = ffmpeg_create_decoder(dx11State, "../../../../480p.mp4");
        //FFMpegPerVideoState ffmpeg2160 = ffmpeg_create_decoder(dx11State, "../../../../2160p.mkv");

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
                if (ffmpeg480.latestFrame == nullptr) {
                    ffmpeg480.readFrame();
                }
                dx11State.enqueueRenderAndPresentForNextFrame(ffmpeg480.latestFrame);
            }
        }

        // Make sure the command queue has finished all commands before closing.
        ffmpeg480.flushAndClose();
        dx11State.flushAndClose();
    }

    //::CloseHandle(g_dx12State.inflightFrameFenceEvent);

	return 0;
}
