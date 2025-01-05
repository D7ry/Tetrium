#pragma once

#include "DXGIContext.h"

struct DXGIDisplayContext
{
    HWND window;
    uint32_t width;
    uint32_t height;
    DXGI_RATIONAL refreshRate;
    IDXGIOutput* output;
};

namespace DXGI
{
// Obtain a DXGI display context, holding one reference to `device` and `adapter`, the caller 
// is responsible for releasing them.
DXGIDisplayContext PickAndInitDXGIDisplayContext();
void CleanupDXGIDisplayContext(DXGIDisplayContext ctx);
} // namespace DXGI