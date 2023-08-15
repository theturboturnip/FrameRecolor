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
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
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

// from https://learn.microsoft.com/en-us/windows/win32/direct3d11/how-to--compile-a-shader
HRESULT dx11_compile_shader_blob(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob) {
    if (!srcFile || !entryPoint || !profile || !blob)
        return E_INVALIDARG;

    *blob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    flags |= D3DCOMPILE_DEBUG;
#endif

    const D3D_SHADER_MACRO defines[] =
    {
        "EXAMPLE_DEFINE", "1",
        NULL, NULL
    };

    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint, profile,
        flags, 0, &shaderBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }

        if (shaderBlob)
            shaderBlob->Release();

        return hr;
    }

    *blob = shaderBlob;

    return hr;
}

struct DX11VertexShaderStuff {
    ComPtr<ID3D11VertexShader> shader;
    ComPtr<ID3D11InputLayout> inputLayout;
};

DX11VertexShaderStuff dx11_compile_vertex_shader(ComPtr<ID3D11Device>& device, _In_ LPCWSTR srcFile, D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, u32 NumElements) {
    ID3DBlob* blob = nullptr;
    ThrowIfFailed(D3DReadFileToBlob(srcFile, &blob));

    ComPtr<ID3D11VertexShader> vertexShader;
    ThrowIfFailed(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &vertexShader));

    ComPtr<ID3D11InputLayout> inputLayout;
    ThrowIfFailed(device->CreateInputLayout(pInputElementDescs, NumElements, blob->GetBufferPointer(), blob->GetBufferSize(), &inputLayout));

    return {
        .shader = vertexShader,
        .inputLayout = inputLayout
    };
}

ComPtr<ID3D11PixelShader> dx11_compile_pixel_shader(ComPtr<ID3D11Device>& device, _In_ LPCWSTR srcFile) {
    ID3DBlob* blob = nullptr;
    ThrowIfFailed(D3DReadFileToBlob(srcFile, &blob));

    ComPtr<ID3D11PixelShader> pixelShader;
    ThrowIfFailed(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &pixelShader));

    return pixelShader;
}

ComPtr<ID3D11Buffer> dx11_create_buffer(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& deviceContext, const void* srcData, u32 srcDataSizeBytes) {
    auto bufferDesc = D3D11_BUFFER_DESC{
        .ByteWidth = srcDataSizeBytes,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
    };
    ComPtr<ID3D11Buffer> buffer;
    device->CreateBuffer(&bufferDesc, nullptr, &buffer);
    D3D11_MAPPED_SUBRESOURCE ms;
    deviceContext->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, srcData, srcDataSizeBytes);
    deviceContext->Unmap(buffer.Get(), 0);

    return buffer;
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
        .BufferCount = NUM_INFLIGHT_FRAMES,
        .OutputWindow = g_windowState.hWnd,
        .Windowed = true,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
    };

    ComPtr<IDXGISwapChain> swapchain = nullptr;
    ComPtr<ID3D11Device> device = nullptr;
    ComPtr<ID3D11DeviceContext> deviceContext = nullptr;

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
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

    ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(swapchain->GetParent(IID_PPV_ARGS(&factory)))) {
        factory->MakeWindowAssociation(g_windowState.hWnd, DXGI_MWA_NO_WINDOW_CHANGES);
    }

    ComPtr<ID3D11Texture2D> backBufferTex;
    ComPtr<ID3D11RenderTargetView> backBuffer;
    ThrowIfFailed(swapchain->GetBuffer(0, IID_PPV_ARGS(&backBufferTex)));
    ThrowIfFailed(device->CreateRenderTargetView(backBufferTex.Get(), NULL, &backBuffer));

    ComPtr<ID3D11Buffer> quadVertexBuffer = dx11_create_buffer(device, deviceContext, vertex_buffer, sizeof(vertex_buffer));
    ComPtr<ID3D11Buffer> quadIndexBuffer = dx11_create_buffer(device, deviceContext, index_buffer, sizeof(index_buffer));

    // create the input layout object
    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    auto posuv_vert = dx11_compile_vertex_shader(device, L"posuv_vert.cso", ied, 2);

    return DX11State{
        .swapchain = swapchain,
        .device = device,
        .deviceContext = deviceContext,

        .backBuffer = backBuffer,

        .posuv_vert = posuv_vert.shader,
        .posuv_inputlayout = posuv_vert.inputLayout,
        .yuv2RGB_frag = dx11_compile_pixel_shader(device, L"yuv2RGB_frag.cso"),

        .quadVertexBuffer = quadVertexBuffer,
        .quadIndexBuffer = quadIndexBuffer,

        .viewport = {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width = g_windowState.clientWidth * 1.0f,
            .Height = g_windowState.clientHeight * 1.0f,
            .MinDepth = 0,
            .MaxDepth = 1,
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
        ComPtr<ID3D11ShaderResourceView> lastFrameLum = nullptr;
        ComPtr<ID3D11ShaderResourceView> lastFrameChrom = nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC luminance_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(frame.Get(), D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8_UNORM);
        device->CreateShaderResourceView(frame.Get(), &luminance_desc, &lastFrameLum); // DXGI_FORMAT_R8G8_UNORM for NV12 chrominance channel
        D3D11_SHADER_RESOURCE_VIEW_DESC chrominance_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(frame.Get(), D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8_UNORM);
        device->CreateShaderResourceView(frame.Get(), &chrominance_desc, &lastFrameChrom);

        deviceContext->VSSetShader(posuv_vert.Get(), nullptr, 0);
        deviceContext->PSSetShader(yuv2RGB_frag.Get(), nullptr, 0);
        deviceContext->IASetInputLayout(posuv_inputlayout.Get());
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        UINT vertexStride = sizeof(vertex_buffer[0]);
        UINT vertexOffset = 0;
        auto vertexBuffer = quadVertexBuffer.Get();
        deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);
        deviceContext->IASetIndexBuffer(quadIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        ID3D11ShaderResourceView* shaderResources[] = {
            lastFrameLum.Get(),
            lastFrameChrom.Get()
        };
        deviceContext->PSSetShaderResources(0, 2, shaderResources);
        deviceContext->DrawIndexed(6, 0, 0);
        //deviceContext->Vertex
        //deviceContext->CopySubresourceRegion(
        //    backBufferTex.Get(), 0, 0, 0, 0,
        //    frame.Get(), 0, nullptr);
    }

    // switch the back buffer and the front buffer
    swapchain->Present(1, 0);
}
void DX11State::flushAndClose() {
    quadIndexBuffer.Reset();
    quadVertexBuffer.Reset();

    yuv2RGB_frag.Reset();
    posuv_inputlayout.Reset();
    posuv_vert.Reset();

    backBuffer.Reset();

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

    //UINT Width;
    //UINT Height;
    //UINT MipLevels;
    //UINT ArraySize;
    //DXGI_FORMAT Format;
    //DXGI_SAMPLE_DESC SampleDesc;
    //D3D11_USAGE Usage;
    //UINT BindFlags;
    //UINT CPUAccessFlags;
    //UINT MiscFlags;
    auto frameDesc = D3D11_TEXTURE2D_DESC{
        .Width = (u32)state.video_stream->codecpar->width,
        .Height = (u32)state.video_stream->codecpar->height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_NV12,
        .SampleDesc = DXGI_SAMPLE_DESC {
            .Count = 1,
            .Quality = 0
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = 0
    };
    ThrowIfFailed(dx11State.device->CreateTexture2D(&frameDesc, NULL, &state.lastFrameCopyTarget));

    return state;
}

void FFMpegPerVideoState::readFrame(DX11State& dx11State) {
    do {
        av_packet_unref(packet);
        ThrowIfFfmpegFail(av_read_frame(input_ctx, packet));
    } while (packet->stream_index != video_stream_index);

    ThrowIfFfmpegFail(avcodec_send_packet(decoder_ctx, packet));
    
    int ret = avcodec_receive_frame(decoder_ctx, frame);
    switch (ret) {
    case 0: {
        auto latestFrame = (ID3D11Texture2D*)frame->data[0];
        const int texture_index = (int)frame->data[1];
        dx11State.deviceContext->CopySubresourceRegion(
            lastFrameCopyTarget.Get(), 0, 0, 0, 0,
            latestFrame, texture_index, nullptr);
        break;
    }
    case AVERROR_EOF:
    case AVERROR(EAGAIN):
        // Don't copy more stuff around, hopefully frame is still valid with the last frame of the video...
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
        g_dx11Initialized = true;
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
                ffmpeg480.readFrame(dx11State);
                dx11State.enqueueRenderAndPresentForNextFrame(ffmpeg480.lastFrameCopyTarget);
            }
        }

        // Make sure the command queue has finished all commands before closing.
        ffmpeg480.flushAndClose();
        dx11State.flushAndClose();
    }

    //::CloseHandle(g_dx12State.inflightFrameFenceEvent);

	return 0;
}
