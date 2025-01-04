#if defined(WIN32)
#pragma once
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>

#include <tchar.h>
#include <iostream>

struct DXGIDisplayContext
{
    HWND window;
    uint32_t width;
    uint32_t height;
    DXGI_RATIONAL refreshRate;
    ID3D12Device5* device;
    IDXGIAdapter4* adapter;
};
class DXGISwapChain
{
public:
    DXGISwapChain(DXGIDisplayContext& window);
    ~DXGISwapChain();

    HRESULT Create(int count, DXGI_FORMAT format);
    void Present();
    unsigned int GetVBlankCount();


    IDXGISwapChain4* m_pSwapChain;
    ID3D12Device5* m_pDevice;
    IDXGIAdapter4* m_pAdapter;
    ID3D12CommandQueue* m_commandQueue;

    HWND m_hWnd;
    uint32_t m_width;
    uint32_t m_height;
    DXGI_RATIONAL m_refreshRate;
};

namespace DXGI
{
DXGIDisplayContext PickAndInitDXGIDisplayContext();
}

#endif // WIN32