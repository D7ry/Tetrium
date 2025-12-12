#include <iostream>

#include "Tetrium.h"

#include "apps/AppAnomaloscope.h"
#include "apps/AppAutoMeasure.h"
#include "apps/AppImageViewer.h"
#include "apps/AppPainter.h"
#include "apps/AppPseudoIsochromaticTest.h"
// #include "apps/AppScrambledFaceTest.h"  // Commented out - not in use
// #include "apps/AppScreeningTest.h"  // TODO: Refactor to use new TestGenerator interface
#include "apps/AppGeneticTestViewer.h"
#include "apps/AppTemporalAFC.h"
#include "apps/AppTetraHueSphere.h"

static void printGreetingBanner()
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
    // Create ./temp directory if it doesn't exist
    std::system("mkdir -p ./temp");

#if !defined(NDEBUG)
    DEBUG("running in debug mode");
#endif // !NDEBUG

    std::vector<std::pair<TetriumApp::App*, const char*>> apps = {
        // {new TetriumApp::AppScreeningTest(), "Screening Test"},  // TODO: Refactor to use new
        // TestGenerator interface
        {new TetriumApp::AppPseudoIsochromaticTest(), "Pseudoisochromatic Test"},
        // {new TetriumApp::AppScrambledFaceTest(), "Scrambled Face Test"},  // Commented out - not in use
        {new TetriumApp::AppTemporalAFC(), "Temporal 3AFC"},
        {new TetriumApp::AppAnomaloscope(), "Anomaloscope"},
        {new TetriumApp::AppTetraHueSphere(), "Tetra Hue Sphere"},
        {new TetriumApp::AppImageViewer(), "Image Viewer"},
        {new TetriumApp::AppPainter(), "Painter"},
        {new TetriumApp::AppAutoMeasure(), "Measure"},
        {new TetriumApp::AppGeneticTestViewer(), "Genetic Test Viewer"},
    };

#if defined(__APPLE__)
    Tetrium::InitOptions options{.tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync};
#elif defined(_WIN32)
    Tetrium::InitOptions options{.tetraMode = Tetrium::TetraMode::kEvenOddHardwareSync};
#else
    Tetrium::InitOptions options{
        .tetraMode = Tetrium::TetraMode::kEvenOddHardwareSync}; // default or fallback
#endif
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
