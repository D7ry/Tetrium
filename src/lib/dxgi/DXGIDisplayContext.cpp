#if defined(WIN32)
#include <Windows.h>

#include <d3d12.h>
#include <iostream>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi")
#include "backends/imgui_impl_win32.h"


#include "DXGIDisplayContext.h"

using namespace Microsoft::WRL;

// forward declaration for ImGui input + event handling
extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT winEventHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

DXGIDisplayContext DXGI::PickAndInitDXGIDisplayContext()
{
    DXGIDisplayContext ret{};

    auto pAdapter = DXGIContext::adapter;

    std::vector<IDXGIOutput*> outputs;
    std::vector<RECT> outputRects;
    std::cout << "========== Choose Display ==========" << std::endl;

    UINT outputIndex = 0;
    IDXGIOutput* pOutput = nullptr;
    while (pAdapter->EnumOutputs(outputIndex, &pOutput) != DXGI_ERROR_NOT_FOUND) {
        outputs.push_back(pOutput);
        outputIndex++;
    }

    if (outputs.empty()) {
        PANIC("GPU is not connected to any display!");
    }

    // Display the outputs to the user
    for (size_t i = 0; i < outputs.size(); ++i) {
        DXGI_OUTPUT_DESC outputDesc;
        outputs.at(i)->GetDesc(&outputDesc);
        std::wstring desc = outputDesc.DeviceName;
        RECT rect = outputDesc.DesktopCoordinates;
        fmt::println(
            "[{}] {} | ({}, {}) - ({}, {})",
            i,
            std::string(desc.begin(), desc.end()),
            rect.left,
            rect.top,
            rect.right,
            rect.bottom
        );
        outputRects.push_back(rect);
    }

    size_t selectedDisplayIndex = 0;
    do {
        fmt::print("Display({}-{}):", 0, outputs.size() - 1);
        std::cin >> selectedDisplayIndex;
    } while (selectedDisplayIndex >= outputs.size());

    pOutput = outputs.at(selectedDisplayIndex);
    RECT monitorCoord = outputRects.at(selectedDisplayIndex);
    
    // Enumerate display modes (resolutions and refresh rates) for the selected display
    std::vector<DXGI_MODE_DESC> displayModes;
    DXGI_FORMAT displayFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Common format
    UINT numModes = 0;
    // Get the count of available modes
    pOutput->GetDisplayModeList(displayFormat, 0, &numModes, nullptr);

    displayModes.resize(numModes);

    // Now get the actual display modes
    pOutput->GetDisplayModeList(displayFormat, 0, &numModes, displayModes.data());
    
    std::cout << "========== Choose Resolution and Refresh Rate ==========" << std::endl;
    for (size_t i = 0; i < displayModes.size(); ++i) {
        const auto& mode = displayModes[i];
        fmt::print(
            "[{}] {}x{} @ {}Hz\n",
            i,
            mode.Width,
            mode.Height,
            mode.RefreshRate.Numerator / mode.RefreshRate.Denominator
        );
    }

    size_t selectedModeIndex = 0;
    do {
        fmt::print("Resolution({}-{}):", 0, displayModes.size() - 1);
        std::cin >> selectedModeIndex;
    } while (selectedModeIndex >= displayModes.size());

    const auto& selectedMode = displayModes[selectedModeIndex];

    // Set the chosen resolution and refresh rate in ret
    ret.width = selectedMode.Width;
    ret.height = selectedMode.Height;
    ret.refreshRate = selectedMode.RefreshRate;
    
    ret.output = pOutput;

    // Create a window for the selected resolution on the chosen adapter
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = winEventHandler;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "Tetrium";
    RegisterClassExA(&wc);

    INFO("monitor coord: {} {}", monitorCoord.left, monitorCoord.top);
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindoww
    ret.window = CreateWindowW(
        L"Tetrium",
        L"Tetrium",
        WS_VISIBLE | WS_POPUP,
        monitorCoord.left,
        monitorCoord.top,
        ret.width,
        ret.height,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (ret.window == nullptr) {
        PANIC("Failed to create window")
    }

    pOutput->AddRef();
    // release resources
    for (auto* output : outputs) {
        output->Release();
    }

    return ret;
}

void DXGI::CleanupDXGIDisplayContext(DXGIDisplayContext ctx)
{
    DestroyWindow(ctx.window);
    ctx.output->Release();
}

#endif // WIN32