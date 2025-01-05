cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

project(Tetrium)

# ---------- Organize files for MS builds ---------- #
if (WIN32)
    set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(SOURCE_DIR "${ROOT_DIR}/src")
    file(GLOB_RECURSE SOURCE_FILES "${SOURCE_DIR}/*.cpp")	
    file(GLOB_RECURSE HEADER_FILES "${SOURCE_DIR}/*.h" "${SOURCE_DIR}/*.hpp")
    list(APPEND SOURCE_FILES ${HEADER_FILES})

    # 对 SourceFile 变量里面的所有文件分类(保留资源管理器的目录结构)
    foreach(fileItem ${SOURCE_FILES})       
        # Get the directory of the source file
        get_filename_component(PARENT_DIR "${fileItem}" DIRECTORY)
        # Remove common directory prefix to make the group
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
        # Make sure we are using windows slashes
        string(REPLACE "/" "\\" GROUP "${GROUP}")
        # Group into "Source Files" and "Header Files"
        set(GROUP "${GROUP}")
        source_group("${GROUP}" FILES "${fileItem}")
    endforeach()

    source_group(TREE "${ROOT_DIR}" FILES ${SOURCE_FILES})
endif()

# ---------- Configure Apple Settings ---------- #
if (APPLE)
    set(VULKAN_LIB_PATH $ENV{HOME}/lib/VulkanSDK/1.3.290.1/macOS/lib/libvulkan.1.3.290.dylib)
    set(MOLTENVK_LIB_PATH $ENV{HOME}/lib/VulkanSDK/1.3.290.1/macOS/lib/libMoltenVK.dylib)
    MESSAGE(Building in Apple machine.)
    MESSAGE(Vulkan Libary Path: "${VULKAN_LIB_PATH}")
    MESSAGE(MoltenVK Libary Path: "${MOLTENVK_LIB_PATH}")
endif()
 
# ---------- Deps in extern/ ---------- #
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
    extern/imgui/misc/cpp/imgui_stdlib.cpp
    extern/imgui/misc/freetype/imgui_freetype.cpp
    extern/implot/implot.cpp
    extern/implot/implot_items.cpp
)

if (WIN32)
    list(APPEND IMGUI_SRC extern/imgui/backends/imgui_impl_win32.cpp)
endif()

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

# tetriumColor
add_subdirectory(extern/TetriumColor)

# ---------- Linked Libraries ---------- #
# freetype
if (APPLE)
    set(FREETYPE_INCLUDE_DIRS "/opt/homebrew/include/freetype2")
    set(FREETYPE_LIBRARIES "/opt/homebrew/lib/libfreetype.dylib")
    # hard-coded path for openal-soft
    set(OPENAL_ROOT_DIR "/opt/homebrew/opt/openal-soft")
    set(OPENAL_INCLUDE_DIR "${OPENAL_ROOT_DIR}/include")
    set(OPENAL_LIBRARY "${OPENAL_ROOT_DIR}/lib/libopenal.dylib")
endif()

set(SOURCES
        src/lib/Utils.cpp
        src/main.cpp
        src/Tetrium_Bootstrap.cpp
        src/Tetrium_GUI.cpp
        src/Tetrium_EvenOdd.cpp
        src/Tetrium_Tick.cpp
        src/Tetrium_Windowing.cpp
        src/Tetrium_Config.cpp
        src/Tetrium_ImGui.cpp
        src/components/TaskQueue.cpp
        src/components/SoundManager.cpp
        src/components/Logging.cpp
        src/components/ShaderUtils.cpp
        src/components/DeltaTimer.cpp
        src/components/Camera.cpp
        src/components/TextureManager.cpp
        src/components/InputManager.cpp
        # src/components/imgui_widgets/ImGuiWidgetTemp.cpp
        src/components/imgui_widgets/ImGuiWidgetPerfPlot.cpp
        src/components/imgui_widgets/ImGuiWidgetDeviceInfo.cpp
        # src/components/imgui_widgets/ImGuiWidgetUBOViewer.cpp
        src/components/imgui_widgets/ImGuiWidgetEvenOddCalibration.cpp
        # src/components/imgui_widgets/ImGuiWidgetGraphicsPipeline.cpp
        src/components/imgui_widgets/ImGuiWidgetColorTile.cpp
        src/components/imgui_widgets/ImGuiWidgetBlobHunter.cpp

        src/lib/VulkanUtils.cpp
        src/lib/VQDeviceImage.cpp
        src/lib/VQDevice.cpp
        src/lib/VQUtils.cpp
        src/lib/ImGuiUtils.cpp
        src/structs/Vertex.cpp

        # Apps
        src/apps/AppScreeningTest.cpp
        src/apps/AppTetraHueSphere.cpp
        src/apps/AppImageViewer.cpp
        src/apps/AppPainter.cpp
        src/apps/painter/PainterImGui.cpp
        src/apps/painter/PainterColorPicker.cpp
        src/apps/painter/PainterPaint.cpp
        src/apps/painter/PainterSerialize.cpp

        src/apps/app_components/TextureFrameBuffer.cpp
)

if (WIN32)
    list(APPEND SOURCES src/lib/dxgi/DXGISwapchain.cpp)
    list(APPEND SOURCES src/lib/dxgi/DXGIDisplayContext.cpp)
    list(APPEND SOURCES src/lib/dxgi/DXGIContext.cpp)
endif()


add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} 
    PRIVATE ${IMGUI_SRC}
    PRIVATE ${STB_IMG_SRC}
)

# vulkan
if (UNIX)
    if(APPLE)
        set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
        include_directories(~/lib/VulkanSDK/1.3.290.1/macOS/include)
        target_link_libraries(${PROJECT_NAME} 
            ${VULKAN_LIB_PATH}
            ${MOLTENVK_LIB_PATH}
        )
        target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_MACOS_MVK)
    else()
# linux
    find_package(Vulkan REQUIRED)
    find_package(X11 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DRM REQUIRED libdrm)
    pkg_check_modules(SNDFILE REQUIRED sndfile)
    include_directories(${DRM_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan
        ${X11_LIBRARIES}
        ${DRM_LIBRARIES}
        Xrandr
    )
endif() # LINUX
endif() # APPLE

if (WIN32)
    set(VULKAN_SDK_PATH "C:/VulkanSDK/1.3.290.0")
    set(Vulkan_LIBRARY "${VULKAN_SDK_PATH}/Lib/vulkan-1.lib")
    set(Vulkan_INCLUDE_DIR "${VULKAN_SDK_PATH}/Include")
    find_package(Vulkan REQUIRED)
    include_directories(${Vulkan_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    if(MSVC)
        add_compile_options(/W4 /wd4100 /wd4201)
    endif()

    # DirectX
    find_package(dxsdk-d3dx CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} Microsoft::D3DX9 Microsoft::D3DX10 Microsoft::D3DX11)
endif() # WIN32

find_package(OpenAL CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(SndFile CONFIG REQUIRED)
find_package(TIFF REQUIRED)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${TETRIUMCOLOR_INCLUDE}
    PRIVATE ${STB_INCLUDE}
    PRIVATE ${IMGUI_INCLUDE}
    PRIVATE ${IMPLOT_INCLUDE}
    PRIVATE ${SPDLOG_INCLUDE}
    PRIVATE ${GLFW_INCLUDE}
    PRIVATE ${GLM_INCLUDE}
    PRIVATE ${TINY_OBJ_LOADER_INCLUDE}
    PRIVATE ${FREETYPE_INCLUDE_DIRS}
    PRIVATE ${OPENAL_INCLUDE_DIR}
    PRIVATE ${SNDFILE_INCLUDE_DIR}
    PRIVATE src
)

target_link_libraries(${PROJECT_NAME} 
    spdlog::spdlog 
    glfw
    TetriumColor
    ${FREETYPE_LIBRARIES}
    ${OPENAL_LIBRARY}
    ${SNDFILE_LIBRARY}
    ${TIFF_LIBRARIES}
)

target_precompile_headers(${PROJECT_NAME} PUBLIC src/PCH.h)

# debug flag for unix
if(CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_definitions(NDEBUG)
endif()
