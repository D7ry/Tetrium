#include <iostream>

#include "Tetrium.h"

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
    Tetrium::InitOptions options{
        .tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync,
        .fullScreen = true,
        .manualMonitorSelection = true
    };
    Tetrium engine;
    engine.Init(options);
    engine.Run();
    engine.Cleanup();

    return 0;
}
