#if defined(WIN32)
#include <Windows.h>

#include <d3d12.h>
#include <iostream>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi")
#include "backends/imgui_impl_win32.h"


#include "GlobalStates.h"
#include "DXGIDisplayContext.h"

using namespace Microsoft::WRL;

// forward declaration for ImGui input + event handling
extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ImGui cursor helpers
namespace ImGuiMouse
{
// is the previous frame focused?
bool prevFocused = true;

// handle mouse input and translate such into ImGui mouse delta.
// we do this instead of using ImGui's mouse function because we render
// to a window of different size than the ImGui main viewport.
bool HandleImGuiMouse(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    bool hijackImGui = false;
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE: {
        hijackImGui = true;

        // only process inputs when focused
        if (!GlobalStates::isWindowFocused) {
            prevFocused = false;
            break;
        }

        // calculate center of the window
        RECT rect;
        GetClientRect(hWnd, &rect);
        POINT center = { rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2 };

        // calculate cursor drift from center
        ImGuiIO& io = ImGui::GetIO();
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        int deltaX = 0;
        int deltaY = 0;
        // only calculate delta X and Y if we were previously focused
        if (prevFocused) {
            deltaX = cursorPos.x - center.x;
            deltaY = cursorPos.y - center.y;
        }
        io.MouseDelta = ImVec2(deltaX, deltaY);

        // apply as delta to imgui mouse
        auto pos = io.MousePos;
        ImVec2 newPos = {io.MousePos.x + io.MouseDelta.x, io.MousePos.y + io.MouseDelta.y};
        io.AddMousePosEvent(newPos.x, newPos.y);

        // re-center the OS cursor for new calculation
        ClientToScreen(hWnd, &center);
        SetCursorPos(center.x, center.y);
        
        prevFocused = true;
        break;
    }
    default:
    break;
    }

    return hijackImGui;
}

}

static LRESULT winEventHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    bool hijackImGui = ImGuiMouse::HandleImGuiMouse(hWnd, msg, wParam, lParam);
    bool shouldProcessImGui = !hijackImGui;
    if (shouldProcessImGui 
        && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 0;
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