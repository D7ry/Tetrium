#include <iostream>

#include "Tetrium.h"

#include "apps/AppScreeningTest.h"
#include "apps/AppTetraHueSphere.h"
#include "apps/AppImageViewer.h"
#include "apps/AppPainter.h"

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

    std::vector<std::pair<TetriumApp::App*, const char*>> apps = {
        {new TetriumApp::AppScreeningTest(), "Screening Test"},
        {new TetriumApp::AppTetraHueSphere(), "Tetra Hue Sphere"},
        {new TetriumApp::AppImageViewer(), "Image Viewer"},
        {new TetriumApp::AppPainter(), "Painter"},
    };


    Tetrium::InitOptions options{.tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync};
    Tetrium engine;

    for (auto& [app, appName] : apps) {
        engine.RegisterApp(app, appName);
    }

    engine.Init(options);
    engine.Run();
    engine.Cleanup();


    for (auto& [app, appName] : apps) {
        delete app;
    }

    return 0;
}
