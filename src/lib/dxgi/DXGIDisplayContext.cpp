#if defined(WIN32)

#include <iostream>
#include <d3d12.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi")

#include "DXGIDisplayContext.h"

using namespace Microsoft::WRL;
// TODO: we manually choose the GPU here, but the Vulkan chooses its own GPU(it prefers discrete GPU,
// if they choose different GPUs it may be a problem. Maybe should just enforce a constraint of having one discrete GPU 
// and automatically have both DXGI and Vulkan choose the same GPU.

DXGIDisplayContext DXGI::PickAndInitDXGIDisplayContext()
{
    DXGIDisplayContext ret{};
    std::vector<IDXGIAdapter*> adapters;

    IDXGIFactory1* pFactory = DXGIContext::factory7;

    UINT adapterIndex = 0;
    IDXGIAdapter* pAdapter = nullptr;

    // iterate through all adaptors
    std::cout << "========== Choose GPU ==========" << std::endl;

    while (pFactory->EnumAdapters(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        adapters.push_back(pAdapter);
        adapterIndex++;
    }

    for (size_t i = 0; i < adapters.size(); ++i) {
        DXGI_ADAPTER_DESC adapterDesc;
        adapters.at(i)->GetDesc(&adapterDesc);
        std::wstring desc = adapterDesc.Description;
        fmt::println("[{}] {}", i, std::string(desc.begin(), desc.end()));
    }

    size_t selectedIndex = 0;
    do {
        fmt::print("GPU({}-{}):", 0, adapters.size() - 1);
        std::cin >> selectedIndex;
    } while (selectedIndex >= adapters.size());

    pAdapter = adapters.at(selectedIndex);
    pAdapter->AddRef(); // keep the selected adapter alive, as we decref all adapters at the end.

    std::vector<IDXGIOutput*> outputs;
    std::cout << "========== Choose Display ==========" << std::endl;

    UINT outputIndex = 0;
    IDXGIOutput* pOutput = nullptr;
    while (pAdapter->EnumOutputs(outputIndex, &pOutput) != DXGI_ERROR_NOT_FOUND) {
        outputs.push_back(pOutput);
        outputIndex++;
    }
	
	if (outputs.empty()) {
        PANIC("Selected GPU is not connected to any display!");
    }

    // Display the outputs to the user
    for (size_t i = 0; i < outputs.size(); ++i) {
        DXGI_OUTPUT_DESC outputDesc;
        outputs.at(i)->GetDesc(&outputDesc);
        std::wstring desc = outputDesc.DeviceName;
        fmt::println("[{}] {}", i, std::string(desc.begin(), desc.end()));
    }

    size_t selectedDisplayIndex = 0;
    do {
        fmt::print("Display({}-{}):", 0, outputs.size() - 1);
        std::cin >> selectedDisplayIndex;
    } while (selectedDisplayIndex >= outputs.size());

    pOutput = outputs.at(selectedDisplayIndex);

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
	
    // Create a window for the selected resolution on the chosen adapter
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        return 0;
    };
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "Tetrium";
    RegisterClassEx(&wc);
	
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindoww
    ret.window = CreateWindowW(
        L"Tetrium",
        L"Tetrium",
        WS_VISIBLE | WS_POPUP,
        0,
        0,
        ret.width,
        ret.height,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

	if (ret.window == nullptr) {
        PANIC("Failed to create window");
    }

    auto res = D3D12CreateDevice(
        pAdapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&ret.device)
    );

	if (res != 0) {
        PANIC("Failed to create device");
    }

    // release resources
	for (auto* output : outputs) {
        output->Release();
    }
    for (auto* adapter : adapters) {
        adapter->Release();
    }
	
    return ret;
}
#endif // WIN32