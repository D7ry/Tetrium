#if defined(WIN32)
#include "DXGIContext.h"
#include "dxgi.h"
#include "dxgi1_6.h"

namespace DXGIContext
{
void Init()
{
    INFO("Initializing DXGI context");
    // factory
    DX_CHECK(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory7)))
    ASSERT(factory7 != nullptr)
    
    DX_CHECK(factory7->EnumAdapterByGpuPreference(
        0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)
    ))
    ASSERT(adapter != nullptr)
    
    DX_CHECK(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&device)))
    ASSERT(device != nullptr)

    {
        // log GPU info
        DXGI_ADAPTER_DESC1 desc{};
        DX_CHECK(adapter->GetDesc1(&desc));
        auto wGPUName = std::wstring(desc.Description);
        auto GPUName = std::string(wGPUName.begin(), wGPUName.end());
        INFO("GPU: {}", GPUName);
    }
    
}

void Destroy()
{
    device->Release();
    adapter->Release();
    factory7->Release();
}
} // namespace DXGIContext

#endif // WIN32