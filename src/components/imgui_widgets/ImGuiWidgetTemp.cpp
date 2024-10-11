#include "ImGuiWidgetTemp.h"

void ImGuiWidgetTemp::Draw(Tetrium* engine, ColorSpace colorSpace) {
    drawFakeVideo(engine, colorSpace);
}

void ImGuiWidgetTemp::initFakeVideo(Tetrium* engine) {
    const char* RGB_VIDEO_FOLDER_PATH = "../assets/video/alpha/rgb/";
    const char* OCV_VIDEO_FOLDER_PAHT = "../assets/video/alpha/ocv/";

}
// draw a fake video from two folders
void ImGuiWidgetTemp::drawFakeVideo(Tetrium* engine, ColorSpace colorSpace) {
    [[unlikely]] if (!_initialized) {
        initFakeVideo(engine);
        _initialized = true;
    }
    

    

}
