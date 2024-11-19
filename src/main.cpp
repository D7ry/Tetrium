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
    // std::vector<std::string> transformDirs{
    //     "../extern/TetriumColor/TetriumColor/Assets/ColorSpaceTransforms/Neitz_530_559-RGBO"
    // };
    // std::vector<std::string> pregeneratedMetamers{
    //     "../extern/TetriumColor/TetriumColor/Assets/PreGeneratedMetamers/Neitz_530_559-RGBO.pkl"
    // };
    //
    // PseudoIsochromaticPlateGenerator generator(transformDirs, pregeneratedMetamers, 8);
    // generator.NewPlate("plate_RGB.png", "plate_OCV.png", 27);

    printGreetingBanner();
    INIT_LOGS();
    INFO("Logger initialized.");
    DEBUG("running in debug mode");
    TetriumColor::Init();

    Tetrium::InitOptions options{.tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync};
    Tetrium engine;
    engine.Init(options);
    engine.Run();
    engine.Cleanup();

    TetriumColor::Cleanup();

    return 0;
}
