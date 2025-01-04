#if defined(WIN32)
#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <iostream>
#pragma comment(lib, "dxgi")

struct DXGIWindow
{
    HWND window;
    uint32_t width;
    uint32_t height;
    DXGI_RATIONAL refreshRate;
};
class DXGISwapChain
{
public:
    DXGISwapChain(HWND hWnd, uint32_t width, uint32_t height);
    ~DXGISwapChain();

    HRESULT Create();
    void Present();

    IDXGISwapChain* m_pSwapChain;
    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    HWND m_hWnd;
    uint32_t m_width;
    uint32_t m_height;
};

DXGIWindow PickFullscreenDXGIWindow();

#endif // WIN32