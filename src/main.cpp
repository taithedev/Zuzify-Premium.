#include "app.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static IDXGISwapChain* g_swapChain = nullptr;
static ID3D11RenderTargetView* g_target = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static void CleanupRenderTarget() {
    if (g_target) { g_target->Release(); g_target = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        g_device->CreateRenderTargetView(backBuffer, nullptr, &g_target);
        backBuffer->Release();
    }
}

static bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    return SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &level, &g_context));
}

static void CleanupDevice() {
    CleanupRenderTarget();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    g_swapChain = nullptr; g_context = nullptr; g_device = nullptr;
}

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    if (msg == WM_SIZE && g_device && wp != SIZE_MINIMIZED) {
        CleanupRenderTarget();
        g_swapChain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
        CreateRenderTarget();
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr, L"ZuzifyPremium", nullptr};
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Zuzify Premium", WS_OVERLAPPEDWINDOW,
        100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDevice(hwnd)) {
        CleanupDevice();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    ZuzifyApp app;

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        app.Render();

        ImGui::Render();
        const float clear[4] = {0.012f, 0.012f, 0.018f, 1.0f};
        g_context->OMSetRenderTargets(1, &g_target, nullptr);
        g_context->ClearRenderTargetView(g_target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDevice();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
