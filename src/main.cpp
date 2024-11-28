#include <iostream>

#include "Tetrium.h"
#include "TetriumColor/TetriumColor.h"

#include "apps/AppScreeningTest.h"
#include "apps/AppTetraHueSphere.h"
#include "apps/AppImageViewer.h"

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
    DEBUG("running in debug mode");

    std::vector<std::pair<TetriumApp::App*, const char*>> apps = {
        {new TetriumApp::AppScreeningTest(), "Screening Test"},
        {new TetriumApp::AppTetraHueSphere(), "Tetra Hue Sphere"},
        {new TetriumApp::AppImageViewer(), "Image Viewer"},
    };

    TetriumColor::Init();

    Tetrium::InitOptions options{.tetraMode = Tetrium::TetraMode::kEvenOddHardwareSync};
    Tetrium engine;

    for (auto& [app, appName] : apps) {
        engine.RegisterApp(app, appName);
    }

    engine.Init(options);
    engine.Run();
    engine.Cleanup();

    TetriumColor::Cleanup();

    for (auto& [app, appName] : apps) {
        delete app;
    }

    return 0;
}
