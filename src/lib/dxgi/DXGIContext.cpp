#if defined(WIN32)
#include "DXGIContext.h"
namespace DXGIContext
{
void Init() { 
	INFO("Initializing DXGI context");
    ASSERT(SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory4))))


}
    

    void Destroy() { factory4->Release();
    }
}

#endif // WIN32