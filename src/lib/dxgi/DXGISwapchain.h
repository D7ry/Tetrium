#if defined(WIN32)
#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <iostream>

struct DXGISwapchainCreateContext
{
    HWND window;
    uint32_t width;
    uint32_t height;
    DXGI_RATIONAL refreshRate;
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;
    IDXGIAdapter* adapter;
};
class DXGISwapChain
{
public:
    DXGISwapChain(DXGISwapchainCreateContext& window);
    ~DXGISwapChain();

    HRESULT Create();
    void Present();
    unsigned int GetVBlankCount();

 private:

    IDXGISwapChain* m_pSwapChain;
    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;
    IDXGIAdapter* m_pAdapter;

    HWND m_hWnd;
    uint32_t m_width;
    uint32_t m_height;
    DXGI_RATIONAL m_refreshRate;
};

namespace DXGI
{
DXGISwapChain PickDisplayAndCreateSwapchain();
}

#endif // WIN32