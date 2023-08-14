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
DX11State g_dx11State;
bool g_dx11Initialized = false;

// Window callback function.
LRESULT CALLBACK window_message_callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_dx11Initialized) return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    switch (uMsg) {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;
    case WM_PAINT:
        g_dx11State.enqueueRenderAndPresentForNextFrame();
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

void dx11_init() {
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
}

void DX11State::enqueueRenderAndPresentForNextFrame() {

}
void DX11State::flushAndClose() {
    // This can really be taken care of by destructors...
    deviceContext.Reset();
    device.Reset();
    swapchain.Reset();
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
    dx11_init();
    g_dx11Initialized = true;

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
            g_dx11State.enqueueRenderAndPresentForNextFrame();
        }
    }

    // Make sure the command queue has finished all commands before closing.
    g_dx11State.flushAndClose();

    //::CloseHandle(g_dx12State.inflightFrameFenceEvent);

	return 0;
}
