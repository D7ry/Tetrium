#if defined(WIN32)
#include "DXGISwapchain.h"

DXGISwapChain::DXGISwapChain(HWND hWnd, uint32_t width, uint32_t height)
    : m_hWnd(hWnd),
      m_width(width),
      m_height(height),
      m_pSwapChain(nullptr),
      m_pDevice(nullptr),
      m_pDeviceContext(nullptr)
{
}

DXGISwapChain::~DXGISwapChain()
{
    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pDeviceContext) {
        m_pDeviceContext->Release();
        m_pDeviceContext = nullptr;
    }

    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
}


HRESULT DXGISwapChain::Create()
{
    HRESULT hr = S_OK;

    // Create a factory
    IDXGIFactory* pFactory = nullptr;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&pFactory);
    if (FAILED(hr))
        return hr;

    // Define swap chain description
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = m_width;
    sd.BufferDesc.Height = m_height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = false; // always full-screen

    // Create device and swap chain
    hr = pFactory->CreateSwapChain(nullptr, &sd, &m_pSwapChain);
    if (FAILED(hr)) {
        pFactory->Release();
        return hr;
    }

    // Obtain DXGI factory from swap chain.
    IDXGIDevice* pDXGIDevice = nullptr;
    hr = m_pSwapChain->GetDevice(__uuidof(IDXGIDevice), (void**)&pDXGIDevice);
    if (FAILED(hr)) {
        pFactory->Release();
        return hr;
    }

    // Create device and device context.
    hr = pDXGIDevice->GetParent(__uuidof(ID3D11Device), (void**)&m_pDevice);
    if (FAILED(hr)) {
        pFactory->Release();
        pDXGIDevice->Release();
        return hr;
    }

    m_pDevice->GetImmediateContext(&m_pDeviceContext);

    // Release interfaces
    pFactory->Release();
    pDXGIDevice->Release();

    return S_OK;
}

void DXGISwapChain::Present() { m_pSwapChain->Present(1, 0); }



DXGIWindow PickFullscreenWindow()
{
    DXGIWindow ret{};
    std::vector<IDXGIAdapter*> adapters;

    IDXGIFactory1* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory))) {
        PANIC("Failed to create DXGI Factory.")
    }

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
    wc.lpszClassName = "FullscreenWindow";
    RegisterClassEx(&wc);
	
    ret.window = CreateWindowExW(
         0,
         L"FullscreenWindow",
         L"FullscreenWindow",
         WS_POPUP,
         0,
         0,
         ret.width,
         ret.height,
         nullptr,
         nullptr,
         wc.hInstance,
         nullptr
    );

	if (ret.window == nullptr) {
        PANIC("Failed to create window");
    }

    // release resources
	for (auto* output : outputs) {
        output->Release();
    }
    for (auto* adapter : adapters) {
        adapter->Release();
    }
    pFactory->Release();
    while (1) {
        continue;
    }
	
    return ret;

}

#endif // WIN32