#include <iostream>

#include "Tetrium.h"
#include "MetalInterface.h"

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

    int i =0 ;
    //int res = pObjectC(i);

    printGreetingBanner();
    INIT_LOGS();
    INFO("Logger initialized.");
    INFO("{} -> {}",i, res);
    DEBUG("running in debug mode");
    Tetrium::InitOptions options{
        .tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync
    };
    Tetrium engine;
    engine.Init(options);
    engine.Run();
    engine.Cleanup();

    return 0;
}
