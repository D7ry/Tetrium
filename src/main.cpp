#include <iostream>

#include "Tetrium.h"
#include "TetriumColor/TetriumColor.h"

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
    };

    TetriumColor::Init();

    Tetrium::InitOptions options{.tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync};
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
