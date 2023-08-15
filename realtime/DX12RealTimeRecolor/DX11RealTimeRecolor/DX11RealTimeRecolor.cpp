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

ComPtr<ID3D11ComputeShader> dx11_compile_compute_shader(ComPtr<ID3D11Device>& device, _In_ LPCWSTR srcFile) {
    ID3DBlob* blob = nullptr;
    ThrowIfFailed(D3DReadFileToBlob(srcFile, &blob));

    ComPtr<ID3D11ComputeShader> computeShader;
    ThrowIfFailed(device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &computeShader));

    return computeShader;
}

ComPtr<ID3D11Buffer> dx11_create_buffer(ComPtr<ID3D11Device>& device, UINT BindFlags, const void* srcData, u32 srcDataSizeBytes) {
    auto bufferDesc = D3D11_BUFFER_DESC{
        .ByteWidth = srcDataSizeBytes,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = BindFlags,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
    };
    auto subresourceData = D3D11_SUBRESOURCE_DATA{
        .pSysMem = srcData,
        .SysMemPitch = srcDataSizeBytes,
        .SysMemSlicePitch = 0
    };
    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(device->CreateBuffer(&bufferDesc, &subresourceData, &buffer));
    return buffer;
}

void dx11_write_buffer(ComPtr<ID3D11DeviceContext>& deviceContext, ComPtr<ID3D11Buffer>& buffer, const void* srcData, u32 srcDataSizeBytes) {
    D3D11_MAPPED_SUBRESOURCE ms;
    deviceContext->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, srcData, srcDataSizeBytes);
    deviceContext->Unmap(buffer.Get(), 0);
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

    ComPtr<ID3D11Buffer> quadVertexBuffer = dx11_create_buffer(device, D3D11_BIND_VERTEX_BUFFER, vertex_buffer, sizeof(vertex_buffer));
    ComPtr<ID3D11Buffer> quadIndexBuffer = dx11_create_buffer(device, D3D11_BIND_INDEX_BUFFER, index_buffer, sizeof(index_buffer));

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
        .yuv_bt601_to_rgb_frag = dx11_compile_pixel_shader(device, L"yuv_bt601_to_rgb_frag.cso"),
        .yuv_bt601_to_rgb_comp = dx11_compile_compute_shader(device, L"yuv_bt601_to_rgb_comp.cso"),
        .rgb_frag = dx11_compile_pixel_shader(device, L"rgb_frag.cso"),
        .yuv_rec2020_to_cielab_comp = dx11_compile_compute_shader(device, L"yuv_rec2020_to_cielab_comp.cso"),

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

void DX11State::enqueueRenderAndPresentForNextFrame(ComPtr<ID3D11ShaderResourceView> frame) {
    ID3D11RenderTargetView* targets[] = { backBuffer.Get() };
    deviceContext->OMSetRenderTargets(1, targets, NULL);
    deviceContext->RSSetViewports(1, &viewport);

    // clear the back buffer
    FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
    deviceContext->ClearRenderTargetView(backBuffer.Get(), clearColor);

    // copy the frame in
    if (frame) {
        deviceContext->VSSetShader(posuv_vert.Get(), nullptr, 0);
        deviceContext->PSSetShader(rgb_frag.Get(), nullptr, 0);
        deviceContext->IASetInputLayout(posuv_inputlayout.Get());
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        UINT vertexStride = sizeof(vertex_buffer[0]);
        UINT vertexOffset = 0;
        auto vertexBuffer = quadVertexBuffer.Get();
        deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);
        deviceContext->IASetIndexBuffer(quadIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        ID3D11ShaderResourceView* shaderResources[] = {
            frame.Get(),
        };
        deviceContext->PSSetShaderResources(0, 1, shaderResources);
        deviceContext->DrawIndexed(6, 0, 0);
        // Unbind resources for FFMPEG stuff to use
        shaderResources[0] = nullptr;
        deviceContext->PSSetShaderResources(0, 1, shaderResources);
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

    yuv_rec2020_to_cielab_comp.Reset();
    rgb_frag.Reset();
    yuv_bt601_to_rgb_comp.Reset();
    yuv_bt601_to_rgb_frag.Reset();
    posuv_inputlayout.Reset();
    posuv_vert.Reset();

    backBuffer.Reset();

    deviceContext.Reset();
    device.Reset();
    swapchain.Reset();
}

// Adapted from ff_dxva2_common_frame_params in ffmpeg internals: https://github.com/FFmpeg/FFmpeg/blob/8653dcaf7d665b15b40ea9a560c8171b0914a882/libavcodec/dxva2.c#L476
void get_internal_dx11_tex_stats(AVCodecContext* avctx, FfmpegInternalTextureStats* out)
{
    int surface_alignment, num_surfaces;

    /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
    but it causes issues for H.264 on certain AMD GPUs..... */
    if (avctx->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        surface_alignment = 32;
    /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
    all coding features have enough room to work with */
    else if (avctx->codec_id == AV_CODEC_ID_HEVC || avctx->codec_id == AV_CODEC_ID_AV1)
        surface_alignment = 128;
    else
        surface_alignment = 16;

    /* 1 base work surface */
    num_surfaces = 1;

    /* add surfaces based on number of possible refs */
    if (avctx->codec_id == AV_CODEC_ID_H264 || avctx->codec_id == AV_CODEC_ID_HEVC)
        num_surfaces += 16;
    else if (avctx->codec_id == AV_CODEC_ID_VP9 || avctx->codec_id == AV_CODEC_ID_AV1)
        num_surfaces += 8;
    else
        num_surfaces += 2;

    out->content_width = avctx->coded_width;
    out->content_height = avctx->coded_height;
    out->surface_width = FFALIGN(avctx->coded_width, surface_alignment);
    out->surface_height = FFALIGN(avctx->coded_height, surface_alignment);
    out->num_surfaces = num_surfaces;
}
int ff_dxva2_common_frame_params(AVCodecContext* avctx, AVBufferRef* hw_frames_ctx)
{
    AVHWFramesContext* frames_ctx = (AVHWFramesContext*)hw_frames_ctx->data;
    AVHWDeviceContext* device_ctx = frames_ctx->device_ctx;

    if (device_ctx->type == AV_HWDEVICE_TYPE_D3D11VA) {
        frames_ctx->format = AV_PIX_FMT_D3D11;
    }
    else {
        return AVERROR(EINVAL);
    }

    FfmpegInternalTextureStats stats;
    get_internal_dx11_tex_stats(avctx, &stats);

    // This isn't calculated inside get_internal_dx11_tex_stats because avctx->sw_pix_fmt is literally only set for get_hw_format.
    frames_ctx->sw_format = avctx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
    frames_ctx->width = stats.surface_width;
    frames_ctx->height = stats.surface_height;
    frames_ctx->initial_pool_size = stats.num_surfaces;

    if (frames_ctx->format == AV_PIX_FMT_D3D11) {
        auto* frames_hwctx = (AVD3D11VAFramesContext*)frames_ctx->hwctx;

        frames_hwctx->BindFlags |= D3D11_BIND_DECODER;
    }

    return 0;
}
// Callback used by 
AVPixelFormat get_hw_format(AVCodecContext* decoder_ctx, const AVPixelFormat* pix_fmts) {
    // Allocate a custom hwframe_ctx so we can set the bind flags on the generated texture.
    // This has to be set *inside get_hw_format* for some fucking reason. See docs for hw_frames_ctx
    AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(decoder_ctx->hw_device_ctx);
    ThrowIfFfmpegFail(ff_dxva2_common_frame_params(decoder_ctx, hw_frames_ref));
    auto* frames_ctx = (AVHWFramesContext*)hw_frames_ref->data;
    auto* frames_ctx_hw_ctx = (AVD3D11VAFramesContext*)frames_ctx->hwctx;
    frames_ctx_hw_ctx->BindFlags |= D3D11_BIND_UNORDERED_ACCESS; // Allow us to read the texture directly with shader resources and unordered access things
    ThrowIfFfmpegFail(av_hwframe_ctx_init(hw_frames_ref));
    decoder_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);

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
    state.video_stream_index = ThrowIfFfmpegFail(av_find_best_stream(state.input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &state.decoder, 0));
    state.video_stream = state.input_ctx->streams[state.video_stream_index];

    // Allocate a decoder context, fill it with parameters for the video codec we want and the hardware device context...
    state.decoder_ctx = avcodec_alloc_context3(state.decoder);
    assert(state.decoder_ctx);
    // Set up the decoder with the correct parameters for this video stream's codec
    ThrowIfFfmpegFail(avcodec_parameters_to_context(state.decoder_ctx, state.video_stream->codecpar));
    // Request the D3D11 format
    state.decoder_ctx->get_format = get_hw_format;
    //// ... I think this means there's only one DX11 frame texture, and it gets reused?
    //av_opt_set_int(state.decoder_ctx, "refcounted_frames", 1, 0);
    // Create a hardware device context using the DX11 device, put it into the decoder_ctx
    {
        AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        AVHWDeviceContext* device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
        AVD3D11VADeviceContext* d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
        d3d11va_device_ctx->device = dx11State.device.Get();
        state.decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        ThrowIfFfmpegFail(av_hwdevice_ctx_init(state.decoder_ctx->hw_device_ctx));

        av_buffer_unref(&hw_device_ctx);
    }

    // Decoder context now has all parameters filled in, now actually open the decoder
    ThrowIfFfmpegFail(avcodec_open2(state.decoder_ctx, state.decoder, NULL));

    state.packet = av_packet_alloc();
    state.frame = av_frame_alloc();

    get_internal_dx11_tex_stats(state.decoder_ctx, &state.stats);

    state.regionToCopy = D3D11_BOX{
        .left = 0,
        .top = 0,
        .front = 0,

        .right = state.stats.content_width,
        .bottom = state.stats.content_height,
        .back = 1
    };

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
    auto frameRgbDesc = D3D11_TEXTURE2D_DESC{
        .Width = state.stats.content_width,
        .Height = state.stats.content_height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = DXGI_SAMPLE_DESC {
            .Count = 1,
            .Quality = 0
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        .CPUAccessFlags = 0,
        .MiscFlags = 0
    };
    ThrowIfFailed(dx11State.device->CreateTexture2D(&frameRgbDesc, NULL, &state.latestFrameAsRgb));
    ThrowIfFailed(dx11State.device->CreateShaderResourceView(state.latestFrameAsRgb.Get(), nullptr, &state.latestFrameAsRgbSrv));
    ThrowIfFailed(dx11State.device->CreateUnorderedAccessView(state.latestFrameAsRgb.Get(), nullptr, &state.latestFrameAsRgbUav));

    auto frameLabDesc = D3D11_TEXTURE2D_DESC{
        .Width = state.stats.content_width,
        .Height = state.stats.content_height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT, // The A32 is needed because otherwise it doesn't work as a UAV
        .SampleDesc = DXGI_SAMPLE_DESC {
            .Count = 1,
            .Quality = 0
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        .CPUAccessFlags = 0,
        .MiscFlags = 0
    };
    ThrowIfFailed(dx11State.device->CreateTexture2D(&frameLabDesc, NULL, &state.latestFrameAsLab));
    ThrowIfFailed(dx11State.device->CreateShaderResourceView(state.latestFrameAsLab.Get(), nullptr, &state.latestFrameAsLabSrv));
    ThrowIfFailed(dx11State.device->CreateUnorderedAccessView(state.latestFrameAsLab.Get(), nullptr, &state.latestFrameAsLabUav));

    DX11ColorspaceConstantBuffer buf = {
        .texDims = DirectX::XMUINT2(state.stats.content_width, state.stats.content_height),
    };
    state.texDimConstantBuffer = dx11_create_buffer(dx11State.device, D3D11_BIND_CONSTANT_BUFFER, &buf, sizeof(buf));

    return state;
}

void FFMpegPerVideoState::updateBackingFrame(DX11State& dx11State, ID3D11Texture2D* newBackingFrame) {
    if (latestBackingFrame.Get() != newBackingFrame) {
        latestBackingFrame = newBackingFrame;

        D3D11_TEXTURE2D_DESC desc;
        latestBackingFrame->GetDesc(&desc);

        backingFrameUavs.clear();
        for (u32 i = 0; i < desc.ArraySize; i++) {
            auto uavs = BackingFrameUAVs{};

            auto uavLumDesc = D3D11_UNORDERED_ACCESS_VIEW_DESC{
                .Format = DXGI_FORMAT_R8_UNORM,
                .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY,
                .Texture2DArray = {
                    .MipSlice = 0,
                    .FirstArraySlice = i,
                    .ArraySize = 1,
                }
            };
            auto uavChromDesc = D3D11_UNORDERED_ACCESS_VIEW_DESC{
                .Format = DXGI_FORMAT_R8G8_UNORM,
                .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY,
                .Texture2DArray = {
                    .MipSlice = 0,
                    .FirstArraySlice = i,
                    .ArraySize = 1,
                }
            };

            if (desc.Format == DXGI_FORMAT_P010) {
                uavLumDesc.Format = DXGI_FORMAT_R16_UNORM;
                uavChromDesc.Format = DXGI_FORMAT_R16G16_UNORM;
            }

            ThrowIfFailed(dx11State.device->CreateUnorderedAccessView(newBackingFrame, &uavLumDesc, &uavs.lum));
            ThrowIfFailed(dx11State.device->CreateUnorderedAccessView(newBackingFrame, &uavChromDesc, &uavs.chrom));

            backingFrameUavs.push_back(uavs);
        }
    }
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
        auto newBackingFrame = (ID3D11Texture2D*)frame->data[0];
        updateBackingFrame(dx11State, newBackingFrame);

        const int texture_index = (intptr_t)frame->data[1];
        DX11ColorspaceConstantBuffer buf = {
            .texDims = DirectX::XMUINT2((u32)decoder_ctx->width, (u32)decoder_ctx->height),
        };
        dx11_write_buffer(dx11State.deviceContext, texDimConstantBuffer, &buf, sizeof(buf));

        dx11State.deviceContext->CSSetShader(dx11State.yuv_bt601_to_rgb_comp.Get(), nullptr, 0);
        ID3D11UnorderedAccessView* uavs[] = {
            backingFrameUavs[texture_index].lum.Get(),
            backingFrameUavs[texture_index].chrom.Get(),
            latestFrameAsRgbUav.Get()
        };
        dx11State.deviceContext->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
        auto* cbuf = texDimConstantBuffer.Get();
        dx11State.deviceContext->CSSetConstantBuffers(0, 1, &cbuf);
        dx11State.deviceContext->Dispatch(buf.texDims.x, buf.texDims.y, 1);

        dx11State.deviceContext->CSSetShader(dx11State.yuv_rec2020_to_cielab_comp.Get(), nullptr, 0);
        uavs[2] = latestFrameAsLabUav.Get();
        dx11State.deviceContext->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
        dx11State.deviceContext->Dispatch(buf.texDims.x, buf.texDims.y, 1);

        // Unbind resources for other rendering to use
        uavs[0] = nullptr;
        uavs[1] = nullptr;
        uavs[2] = nullptr;
        dx11State.deviceContext->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);


        //dx11State.deviceContext->CopySubresourceRegion(
        //    lastFrameCopyTarget.Get(), 0, // subresource#0 of lastFrameCopyTarget
        //    0, 0, 0, // DstXYZ
        //    latestFrame, texture_index,
        //    &regionToCopy
        //);
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
        FFMpegPerVideoState ffmpeg2160 = ffmpeg_create_decoder(dx11State, "../../../../2160p.mkv");

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
                ffmpeg2160.readFrame(dx11State);
                dx11State.enqueueRenderAndPresentForNextFrame(ffmpeg2160.latestFrameAsRgbSrv);
            }
        }

        // Make sure the command queue has finished all commands before closing.
        ffmpeg2160.flushAndClose();
        ffmpeg480.flushAndClose();
        dx11State.flushAndClose();
    }

    //::CloseHandle(g_dx12State.inflightFrameFenceEvent);

	return 0;
}
