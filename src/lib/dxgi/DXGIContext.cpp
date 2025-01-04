#if defined(WIN32)
#include "DXGIContext.h"
namespace DXGIContext
{
void Init() { 
	INFO("Initializing DXGI context");
    ASSERT(SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory7))))


}
    

    void Destroy() { factory7->Release();
    }
}

#endif // WIN32