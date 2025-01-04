#include <iostream>

#include "Tetrium.h"

#include "apps/AppScreeningTest.h"
#include "apps/AppTetraHueSphere.h"
#include "apps/AppImageViewer.h"
#include "apps/AppPainter.h"

#include "D3dx9math.h"
#include "lib/dxgi/DXGISwapchain.h"
void printGreetingBanner()
{
    // cool banner
    const char* asciiLine = "--------------------------------";

    std::cout << asciiLine << std::endl;
    std::cout << DEFAULTS::Engine::BANNER_TEXT << std::endl;
    std::cout << asciiLine << std::endl;
}

int main(int argc, char** argv)
{
    printGreetingBanner();
    INIT_LOGS();
    INFO("Logger initialized.");

#if !defined(NDEBUG)
    DEBUG("running in debug mode");
#endif // !NDEBUG

    auto swapChain = DXGI::PickDisplayAndCreateSwapchain();
    // Game loop
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Clear the back buffer (replace with your rendering logic)
        //swapChain.m_pDeviceContext->ClearRenderTargetView(nullptr, D3DXCOLOR(0.0f, 0.2f, 0.4f, 1.0f));

        // Present the back buffer
        swapChain.Present();
    }
    

    exit(0);

    std::vector<std::pair<TetriumApp::App*, const char*>> apps = {
        {new TetriumApp::AppScreeningTest(), "Screening Test"},
        {new TetriumApp::AppTetraHueSphere(), "Tetra Hue Sphere"},
        {new TetriumApp::AppImageViewer(), "Image Viewer"},
        {new TetriumApp::AppPainter(), "Painter"},
    };


    Tetrium::InitOptions options{.tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync};
    Tetrium* engine = new Tetrium();

    for (auto& [app, appName] : apps) {
        engine->RegisterApp(app, appName);
    }

    engine->Init(options);
    engine->Run();
    engine->Cleanup();


    for (auto& [app, appName] : apps) {
        delete app;
    }
	
	delete engine;

    return 0;
}
