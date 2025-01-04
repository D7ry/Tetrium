#if defined(WIN32)
#include "DXGIContext.h"


#include "DXGISwapchain.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi")

// https://github.com/krOoze/Hello_Triangle/blob/dxgi_interop/src/WSI/DxgiWsi.h#L634

DXGISwapChain::DXGISwapChain(DXGISwapchainCreateContext& window)
    : m_hWnd(window.window),
      m_width(window.width),
      m_height(window.height),
      m_refreshRate(window.refreshRate),
      m_pSwapChain(nullptr),
      m_pDevice(window.device),
      m_pAdapter(window.adapter)
{
}

DXGISwapChain::~DXGISwapChain()
{
    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }

	if (m_pAdapter) {
        m_pAdapter->Release();
    }
}


HRESULT DXGISwapChain::Create()
{
    HRESULT hr = S_OK;

    // Create a factory
    IDXGIFactory* pFactory = DXGIContext::factory4;
    if (FAILED(hr)) {
        PANIC("Failed to create DXGI factory");
    }

    // Define swap chain description
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = m_width;
    sd.BufferDesc.Height = m_height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = m_refreshRate;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = false; // always full-screen

    ASSERT(m_pDevice);
    // Create device and swap chain
    hr = pFactory->CreateSwapChain(m_pDevice, &sd, &m_pSwapChain);
    if (FAILED(hr)) {
        PANIC("Failed to create swapchain");
    }
	
    return S_OK;
}

void DXGISwapChain::Present() { 
    m_pSwapChain->Present(1, 0);
}

unsigned int DXGISwapChain::GetVBlankCount()
{
    // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_frame_statistics
    // the manual states that `QueryPerformanceCounter` call updates vblank count, it actually not necessary
	// but we're just keeping it here.
    LARGE_INTEGER buf;
    QueryPerformanceCounter(&buf);
    DXGI_FRAME_STATISTICS stats;
    HRESULT res = m_pSwapChain->GetFrameStatistics(&stats);
    // note we don't ASSERT on the result here as it may fail on the first couple of frames.
    DEBUG("vblank: {}", stats.SyncRefreshCount);
    return stats.SyncRefreshCount;
}



DXGISwapchainCreateContext PickFullscreenDXGIWindow()
{
    DXGISwapchainCreateContext ret{};
    std::vector<IDXGIAdapter*> adapters;

    IDXGIFactory1* pFactory = DXGIContext::factory4;

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

    auto res = D3D11CreateDevice(
        pAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        0,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        0,
        0,
        D3D11_SDK_VERSION,
        &ret.device,
        nullptr,
        &ret.deviceContext
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
    pFactory->Release();
	
    return ret;
}

DXGISwapChain DXGI::PickDisplayAndCreateSwapchain() {
    // Get the window handle for fullscreen mode
    auto window = PickFullscreenDXGIWindow();

    // Create an instance of DXGISwapChain
    DXGISwapChain swapChain(window); // Fullscreen mode

    //// Create the swap chain
    if (FAILED(swapChain.Create())) {
        PANIC("Failed to create swap chain");
    }
    return swapChain;
}


#endif // WIN32