#if defined(WIN32)

#pragma once
#include "dxgi1_6.h"
#include <d3d12.h>
#pragma comment(lib, "dxgi.lib")

namespace DXGIContext
{
    inline IDXGIFactory7* factory7 = nullptr;
    inline ID3D12Device5* device = nullptr;
    inline IDXGIAdapter4* adapter = nullptr;

    void Init();
    void Destroy();
}


#endif