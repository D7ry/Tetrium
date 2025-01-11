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
    unsigned int GetNumDroppedFrames();

    IDXGISwapChain4* m_pSwapChain;
    ID3D12CommandQueue* m_commandQueue;

    HWND m_hWnd;
    IDXGIOutput* m_output;
    uint32_t m_width;
    uint32_t m_height;

    uint32_t m_droppedFrames;
    DXGI_RATIONAL m_refreshRate;

    DXGI_FRAME_STATISTICS m_firstStats;
    bool m_obtainedFirstStats = false;

    bool m_firstPresented = false;
};

#endif // WIN32