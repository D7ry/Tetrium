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

#include "TetriumColor/PseudoIsochromaticPlateGenerator.h"
int main(int argc, char** argv)
{
    PseudoIsochromaticPlateGenerator generator(
        "data/transform_dirs.txt",
        "data/pregenerated_filenames.txt",
        100
    );

    exit(0);
    printGreetingBanner();
    INIT_LOGS();
    INFO("Logger initialized.");
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
