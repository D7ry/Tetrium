#include "Quarkolor.h"

int main(int argc, char** argv)
{
    INIT_LOGS();
    INFO("Logger initialized.");
    DEBUG("running in debug mode");
    Quarkolor::InitOptions options{
        .tetraMode = Quarkolor::TetraMode::kEvenOddSoftwareSync,
        .fullScreen = true,
        .manualMonitorSelection = true};
    Quarkolor engine;
    engine.Init(options);
    engine.Run();
    engine.Cleanup();

    return 0;
}
