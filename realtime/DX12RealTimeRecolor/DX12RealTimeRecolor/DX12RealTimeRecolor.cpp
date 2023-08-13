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

// Global state
std::unique_ptr<WindowState> g_windowState;
std::unique_ptr<DX12State> g_dx12State;
bool g_Initialized = false;

// Window callback function.
LRESULT CALLBACK window_message_callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_Initialized) return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

WindowState create_window(
    const wchar_t* windowClassName,
    HINSTANCE hInst,
    const wchar_t* windowTitle,
    LONG width,
    LONG height
) {
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
        .right=width,
        .bottom=height
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

    return WindowState{
        .hWnd = hWnd,
        .windowSizeWhenWindowed = {
            .left = windowX,
            .top = windowY,
            .right = windowX + windowWidth,
            .bottom = windowY + windowHeight
        }
    };
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    auto args = parse_command_line_args();

#if defined(_DEBUG)
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    g_windowState = std::make_unique<WindowState>(create_window(
        L"DX12WindowClass",
        hInstance,
        L"Real Time Recolor",
        args.requestedWindowWidth,
        args.requestedWindowHeight
    ));

	return 0;
}
