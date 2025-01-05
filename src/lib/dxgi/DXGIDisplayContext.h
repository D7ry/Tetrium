#pragma once

#include "DXGIContext.h"

struct DXGIDisplayContext
{
    HWND window;
    uint32_t width;
    uint32_t height;
    DXGI_RATIONAL refreshRate;
};

namespace DXGI
{
// Obtain a DXGI display context, holding one reference to `device` and `adapter`, the caller 
// is responsible for releasing them.
DXGIDisplayContext PickAndInitDXGIDisplayContext();
} // namespace DXGI