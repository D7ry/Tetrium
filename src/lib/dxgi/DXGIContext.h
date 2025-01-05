#if defined(WIN32)

#pragma once
#include "dxgi1_6.h"
#include <d3d12.h>
#pragma comment(lib, "dxgi.lib")

// Global context for DXGI.
namespace DXGIContext
{
    inline IDXGIFactory7* factory7 = nullptr;

    inline ID3D12Device5* device = nullptr; // Best-performing GPU
    inline IDXGIAdapter4* adapter = nullptr; // Best-performing adaptor

    void Init();
    void Destroy();
}


#endif