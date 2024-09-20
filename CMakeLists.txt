cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
 
# imgui
set(IMPLOT_INCLUDE extern/implot)
set(IMGUI_INCLUDE extern/imgui)
set(IMGUI_SRC 
    extern/imgui/imgui.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_demo.cpp
    extern/imgui/imgui_widgets.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/backends/imgui_impl_glfw.cpp
    extern/imgui/backends/imgui_impl_vulkan.cpp
    extern/implot/implot.cpp
    extern/implot/implot_items.cpp
)

# stb
set(STB_INCLUDE extern/stb)
set(STB_IMG_SRC extern/stb/stb_image_impl.cpp)

# spdlog
add_subdirectory(extern/spdlog)
set(SPDLOG_INCLUDE extern/spdlog/include)

# glfw
add_subdirectory(extern/glfw)
set(GLFW_INCLUDE extern/glfw/include)

# glm
set(GLM_INCLUDE extern/glm)

# tinyobjloader
set(TINY_OBJ_LOADER_INCLUDE extern/tinyobjloader)

project(quarkolor)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/VulkanEngine.cpp
        src/components/Logging.cpp
        src/components/ShaderUtils.cpp
        src/components/DeltaTimer.cpp
        src/components/Camera.cpp
        src/components/VulkanUtils.cpp
        src/components/ImGuiManager.cpp
        src/components/TextureManager.cpp
        src/components/InputManager.cpp
        src/components/imgui_widgets/ImGuiWidgetPerfPlot.cpp
        src/components/imgui_widgets/ImGuiWidgetDeviceInfo.cpp
        src/components/imgui_widgets/ImGuiWidgetUBOViewer.cpp
        src/lib/VQDevice.cpp
        src/lib/VQUtils.cpp
        src/structs/Vertex.cpp
        src/ecs/system/SimpleRenderSystem.cpp
)

target_sources(${PROJECT_NAME} 
    PRIVATE ${IMGUI_SRC}
    PRIVATE ${STB_IMG_SRC}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${STB_INCLUDE}
    PRIVATE ${IMGUI_INCLUDE}
    PRIVATE ${IMPLOT_INCLUDE}
    PRIVATE ${SPDLOG_INCLUDE}
    PRIVATE ${GLFW_INCLUDE}
    PRIVATE ${GLM_INCLUDE}
    PRIVATE ${TINY_OBJ_LOADER_INCLUDE}
    PRIVATE src
)


target_precompile_headers(${PROJECT_NAME} PUBLIC src/PCH.h)

# vulkan
if(APPLE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
    set(VULKAN_LIB_PATH $ENV{HOME}/lib/VulkanSDK/1.3.290.1/macOS/lib/libvulkan.1.3.290.dylib)
    set(MOLTENVK_LIB_PATH $ENV{HOME}/lib/VulkanSDK/1.3.290.1/macOS/lib/libMoltenVK.dylib)
    include_directories(~/lib/VulkanSDK/1.3.290.1/macOS/include)
    target_link_libraries(${PROJECT_NAME} 
        ${VULKAN_LIB_PATH}
        ${MOLTENVK_LIB_PATH}
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_MACOS_MVK)
else() # ~APPLE
    find_package(Vulkan REQUIRED)
    target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
endif() # APPLE

target_link_libraries(${PROJECT_NAME} 
    spdlog::spdlog 
    glfw
)

# debug flag for unix
if(CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_definitions(NDEBUG)
endif()
