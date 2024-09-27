#include "Tetrium.h"

int main(int argc, char** argv)
{
    INIT_LOGS();
    INFO("Logger initialized.");
    DEBUG("running in debug mode");
    Tetrium::InitOptions options{
        .tetraMode = Tetrium::TetraMode::kEvenOddSoftwareSync,
        .fullScreen = true,
        .manualMonitorSelection = true};
    Tetrium engine;
    engine.Init(options);
    engine.Run();
    engine.Cleanup();

    return 0;
}
