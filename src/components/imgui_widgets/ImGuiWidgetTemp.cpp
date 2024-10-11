#include <filesystem>

#include "ImGuiWidgetTemp.h"
#include "Tetrium.h"

void ImGuiWidgetTemp::Draw(Tetrium* engine, ColorSpace colorSpace)
{
    drawFakeVideo(engine, colorSpace);
}

// load in all fake video textures
void ImGuiWidgetTemp::initFakeVideo(Tetrium* engine)
{
    const char* VIDEO_FOLDER_PATHS[] = {"../assets/video/alpha/rgb/", "../assets/video/alpha/ocv/"};

    // count # of frames
    auto it = std::filesystem::directory_iterator(VIDEO_FOLDER_PATHS[RGB]);
    int rgbFileCount
        = std::count_if(begin(it), end(it), [](auto& entry) { return entry.is_regular_file(); });
    it = std::filesystem::directory_iterator(VIDEO_FOLDER_PATHS[OCV]);
    int ocvFileCount
        = std::count_if(begin(it), end(it), [](auto& entry) { return entry.is_regular_file(); });
    ASSERT(rgbFileCount == ocvFileCount);

    int frameCount = rgbFileCount;

    // load in all frames for rgb and ocv color space
    _fakeVideoFrames.resize(frameCount);
    for (ColorSpace cs : {RGB, OCV}) {
        const char* videoFolder = VIDEO_FOLDER_PATHS[cs];
        int idx = 0;
        it = std::filesystem::directory_iterator(videoFolder);
        for (const std::filesystem::directory_entry& entry : it) {
            std::string framePath = entry.path().string();
            _fakeVideoFrames[idx].textures[cs]
                = engine->getOrLoadImGuiTexture(engine->_imguiCtx, framePath);
            idx++;
        }
    }
}

// draw a fake video from two folders
void ImGuiWidgetTemp::drawFakeVideo(Tetrium* engine, ColorSpace colorSpace)
{
    [[unlikely]] if (!_fakeVideoInitialized) {
        initFakeVideo(engine);
        _fakeVideoInitialized = true;
    }
    
}
