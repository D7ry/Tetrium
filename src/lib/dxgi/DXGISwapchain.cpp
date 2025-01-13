#if defined(WIN32)
#include "DXGIContext.h"

#include "DXGISwapchain.h"
#include <wrl/client.h>
using namespace Microsoft::WRL;
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi")

// https://github.com/krOoze/Hello_Triangle/blob/dxgi_interop/src/WSI/DxgiWsi.h#L634

DXGISwapChain::DXGISwapChain(DXGIDisplayContext& window)
    : m_pSwapChain(nullptr),
      m_commandQueue(nullptr),
      m_hWnd(window.window),
      m_width(window.width),
      m_height(window.height),
      m_refreshRate(window.refreshRate),
      m_output(window.output)
{
}

DXGISwapChain::~DXGISwapChain()
{
    if (m_pSwapChain) {
        m_pSwapChain->Release();
    }

    if (m_commandQueue) {
        m_commandQueue->Release();
    }
}

HRESULT DXGISwapChain::Create(uint32_t count, DXGI_FORMAT format)
{
    DXGIDisableVBlankVirtualization();
    HRESULT hr = S_OK;

    IDXGIFactory7* pFactory = DXGIContext::factory7;

    const D3D12_COMMAND_QUEUE_DESC dxQDesc = {
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        D3D12_COMMAND_QUEUE_FLAG_NONE,
    };

    DXGIContext::device->CreateCommandQueue(&dxQDesc, IID_PPV_ARGS(&m_commandQueue));

    DEBUG("Creating swapchain of size {} {}", m_width, m_height);
    const DXGI_SWAP_CHAIN_DESC1 swapchainDesc{
        m_width,
        m_height,
        format,
        FALSE,  // Stereo
        {1, 0}, // Samples
        DXGI_USAGE_RENDER_TARGET_OUTPUT,
        (UINT)count, // image count
        DXGI_SCALING_NONE,
        DXGI_SWAP_EFFECT_FLIP_DISCARD, // discard back buffer
        DXGI_ALPHA_MODE_IGNORE,
    };

    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc{
        m_refreshRate,
        DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE,
        DXGI_MODE_SCALING_UNSPECIFIED, // unspecified to avoid mode change
        false,
    };
    IDXGISwapChain1* swapchain1 = nullptr;
    hr = pFactory->CreateSwapChainForHwnd(
        m_commandQueue, m_hWnd, &swapchainDesc, &fullscreenDesc, m_output, &swapchain1
    );
    if (SUCCEEDED(hr)) {
        IDXGISwapChain3* swapChain3 = nullptr;
        hr = swapchain1->QueryInterface(IID_PPV_ARGS(&swapChain3));
        if (SUCCEEDED(hr)) {
            DXGI_SWAP_CHAIN_DESC1 desc;
            swapChain3->GetDesc1(&desc);
            DEBUG("Swap chain created with resolution: {}x{}", desc.Width, desc.Height);
        }
    }
    ASSERT(swapchain1);

    m_pSwapChain = reinterpret_cast<IDXGISwapChain4*>(swapchain1);
    if (FAILED(hr)) {
        PANIC("Failed to create swapchain");
    }

    DX_CHECK(pFactory->MakeWindowAssociation(m_hWnd, 0));

    m_pSwapChain->SetFullscreenState(true, nullptr);

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ASSERT(m_fenceEvent != nullptr)
    DX_CHECK(DXGIContext::device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))
    waitForPreviousFrame();
    return S_OK;
}

void DXGISwapChain::Present()
{
    const UINT syncInterval = 1;
    DXGI_PRESENT_PARAMETERS presentParams{};
    UINT presentFlags = 0;
    HRESULT res = m_pSwapChain->Present1(syncInterval, presentFlags, &presentParams);
    if (SUCCEEDED(res)) {
        m_firstPresented = true;
    }
}

unsigned int DXGISwapChain::GetVBlankCount()
{
    // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_frame_statistics
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpresentstats#remarks
    DXGI_FRAME_STATISTICS stats;
    HRESULT res = m_pSwapChain->GetFrameStatistics(&stats);
    // note we don't ASSERT on the result here as it may fail on the first couple of frames.
    // DEBUG("vblank: {}", stats.SyncRefreshCount);
    return stats.SyncRefreshCount;
}

unsigned int DXGISwapChain::GetNumDroppedFrames()
{
    if (!m_obtainedFirstStats) {
        if (m_firstPresented) {
            HRESULT res = m_pSwapChain->GetFrameStatistics(&m_firstStats);
            if (SUCCEEDED(res) && m_firstStats.PresentRefreshCount > 0
                && m_firstStats.PresentCount > 0) {
                m_obtainedFirstStats = true;
                INFO("obtained first stats: refreshcount: {} presentcount:{}", m_firstStats.PresentRefreshCount, m_firstStats.PresentCount);
            }
        }
        return 0;
    }

    DXGI_FRAME_STATISTICS stats;
    HRESULT res = m_pSwapChain->GetFrameStatistics(&stats);
    DX_CHECK(res);

    if (stats.PresentRefreshCount < m_firstStats.PresentRefreshCount
        || stats.PresentCount < m_firstStats.PresentCount) {
        INFO("Resetting dropped frames counter!");
        return 0;
    }

    unsigned int vblankDiff = stats.PresentRefreshCount - m_firstStats.PresentRefreshCount;
    unsigned int numPresented = stats.PresentCount - m_firstStats.PresentCount;

    unsigned int ret = vblankDiff - numPresented;
    //INFO("vb delta:{} num presented delta: {} {}", vblankDiff, numPresented, ret);
    //m_firstStats = stats;

    return ret;
}

// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
void DXGISwapChain::waitForPreviousFrame()
{
    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    m_commandQueue->Signal(m_fence, fence);
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        DX_CHECK(m_fence->SetEventOnCompletion(fence, m_fenceEvent))
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

}


#endif // WIN32