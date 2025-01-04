#if defined(WIN32)
#include "DXGIContext.h"


#include "DXGISwapchain.h"
#include <wrl/client.h>
using namespace Microsoft::WRL;
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi")

// https://github.com/krOoze/Hello_Triangle/blob/dxgi_interop/src/WSI/DxgiWsi.h#L634

DXGISwapChain::DXGISwapChain(DXGIDisplayContext& window)
    : m_hWnd(window.window),
      m_width(window.width),
      m_height(window.height),
      m_refreshRate(window.refreshRate),
      m_pSwapChain(nullptr),
      m_pDevice(window.device),
      m_pAdapter(window.adapter)
{
}

DXGISwapChain::~DXGISwapChain()
{
    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }

	if (m_pAdapter) {
        m_pAdapter->Release();
    }
}


HRESULT DXGISwapChain::Create(int count, DXGI_FORMAT format)
{
    HRESULT hr = S_OK;

    IDXGIFactory7* pFactory = DXGIContext::factory7;
    if (FAILED(hr)) {
        PANIC("Failed to create DXGI factory");
    }
    const D3D12_COMMAND_QUEUE_DESC dxQDesc
        = {D3D12_COMMAND_LIST_TYPE_DIRECT,
           D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
           D3D12_COMMAND_QUEUE_FLAG_NONE,
          };
	
	m_pDevice->CreateCommandQueue(&dxQDesc, IID_PPV_ARGS(&m_commandQueue));
	
    const DXGI_SWAP_CHAIN_DESC1 swapchainDesc{
        m_width,
        m_height,
        format,
        FALSE,  // Stereo
        {1, 0}, // Samples
        DXGI_USAGE_RENDER_TARGET_OUTPUT,
        count, // image count
        DXGI_SCALING_NONE,
            DXGI_SWAP_EFFECT_FLIP_DISCARD,
        DXGI_ALPHA_MODE_IGNORE,
        0};
	
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc{
        m_refreshRate,
        DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        DXGI_MODE_SCALING_UNSPECIFIED,
        false,
    };
    ASSERT(m_pDevice);
    IDXGISwapChain1* swapchain1 = nullptr;
    hr = pFactory->CreateSwapChainForHwnd(
        m_commandQueue, m_hWnd, &swapchainDesc, nullptr, nullptr, &swapchain1
    );
	
	ASSERT(swapchain1);
	
	m_pSwapChain = reinterpret_cast<IDXGISwapChain4*>(swapchain1);
    if (FAILED(hr)) {
        PANIC("Failed to create swapchain");
    }

	DX_CHECK(pFactory->MakeWindowAssociation(m_hWnd, 0));
	
    return S_OK;
}

void DXGISwapChain::Present() { 
    DX_CHECK(m_pSwapChain->Present(1, 0)); }

unsigned int DXGISwapChain::GetVBlankCount()
{
    // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_frame_statistics
    // the manual states that `QueryPerformanceCounter` call updates vblank count, it actually not necessary
	// but we're just keeping it here.
    LARGE_INTEGER buf;
    QueryPerformanceCounter(&buf);
    DXGI_FRAME_STATISTICS stats;
    HRESULT res = m_pSwapChain->GetFrameStatistics(&stats);
    // note we don't ASSERT on the result here as it may fail on the first couple of frames.
    DEBUG("vblank: {}", stats.SyncRefreshCount);
    return stats.SyncRefreshCount;
}



#endif // WIN32