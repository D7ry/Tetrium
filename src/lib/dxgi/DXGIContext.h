#if defined(WIN32)

#pragma once
#include "dxgi1_6.h"
#pragma comment(lib, "dxgi.lib")

namespace DXGIContext
{
    inline IDXGIFactory7* factory7 = nullptr;
    void Init();
    void Destroy();
}


#endif