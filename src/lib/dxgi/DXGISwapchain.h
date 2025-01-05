#if defined(WIN32)
#pragma once
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>

#include <tchar.h>
#include <iostream>

#include "DXGIDisplayContext.h"

class DXGISwapChain
{
public:
    DXGISwapChain(DXGIDisplayContext& window);
    ~DXGISwapChain();

    HRESULT Create(uint32_t count, DXGI_FORMAT format);
    void Present();
    unsigned int GetVBlankCount();

    IDXGISwapChain4* m_pSwapChain;
    ID3D12CommandQueue* m_commandQueue;

    HWND m_hWnd;
    IDXGIOutput* m_output;
    uint32_t m_width;
    uint32_t m_height;
    DXGI_RATIONAL m_refreshRate;
};

#endif // WIN32