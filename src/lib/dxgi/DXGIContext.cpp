#if defined(WIN32)
#include "DXGIContext.h"

namespace DXGIContext
{
void Init()
{
    INFO("Initializing DXGI context");
    // factory
    DX_CHECK(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory7)))
    
    DX_CHECK(factory7->EnumAdapterByGpuPreference(
        0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)
    ))

    DX_CHECK(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&device)))
}

void Destroy()
{
    device->Release();
    adapter->Release();
    factory7->Release();
}
} // namespace DXGIContext

#endif // WIN32