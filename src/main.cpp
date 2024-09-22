#include "VulkanEngine.h"

int main(int argc, char** argv)
{
    INIT_LOGS();
    INFO("Logger initialized.");
    DEBUG("running in debug mode");
    VulkanEngine::InitOptions options{
        .tetraMode = VulkanEngine::TetraMode::kEvenOddHardwareSync,
        .fullScreen = false,
        .manualMonitorSelection = true};
    VulkanEngine engine;
    engine.Init(options);
    engine.Run();
    engine.Cleanup();

    return 0;
}
