cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

project(Tetrium)

set(ENV{CONDA_PREFIX} "conda placeholder")

if (APPLE)
    set(IS_APPLE TRUE)
elseif(UNIX)
    set(IS_LINUX TRUE)
elseif(WIN32)
    set(IS_WINDOWS TRUE)
endif()

# ---------- Files ---------- #
set(SOURCES_BUILD
        src/lib/Utils.cpp
        src/main.cpp
        src/Tetrium_Bootstrap.cpp
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
        src/components/imgui_widgets/ImGuiWidgetPerfPlot.cpp
        src/components/imgui_widgets/ImGuiWidgetDeviceInfo.cpp
        src/components/imgui_widgets/ImGuiWidgetEvenOddCalibration.cpp
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
        src/apps/painter/PainterColorPickerTick.cpp
        src/apps/painter/PainterPaint.cpp
        src/apps/painter/PainterSerialize.cpp
        src/apps/painter/HueSphere.cpp

        src/apps/app_components/TextureFrameBuffer.cpp
)

if (WIN32)
    list(APPEND SOURCES_BUILD src/lib/dxgi/DXGISwapchain.cpp)
    list(APPEND SOURCES_BUILD src/lib/dxgi/DXGIDisplayContext.cpp)
    list(APPEND SOURCES_BUILD src/lib/dxgi/DXGIContext.cpp)
endif()

# SOURCE_FILES and HEADER_FILES are used to group files in the IDE,
# primarily used to organize for VS solution explorer
set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SOURCE_DIR "${ROOT_DIR}/src")
file(GLOB_RECURSE SOURCE_FILES "${SOURCE_DIR}/*.cpp")	
file(GLOB_RECURSE HEADER_FILES "${SOURCE_DIR}/*.h" "${SOURCE_DIR}/*.hpp")
 
add_executable(${PROJECT_NAME} ${SOURCES_BUILD} ${HEADER_FILES})

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

# Windows uses win32 backend, linux + apple use glfw
if (IS_WINDOWS)
    list(APPEND IMGUI_SRC extern/imgui/backends/imgui_impl_win32.cpp)
else() # Apple or LINUX
    list(APPEND IMGUI_SRC extern/imgui/backends/imgui_impl_glfw.cpp)
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

# ---------- Vulkan ---------- #
if (DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
else()
    message(FATAL_ERROR "VULKAN_SDK is not set, please configure this environment variable to point to the Vulkan SDK")
endif()

target_sources(${PROJECT_NAME} 
    PRIVATE ${IMGUI_SRC}
    PRIVATE ${STB_IMG_SRC}
)

# ---------- Platform-Specific Libs ---------- #
if(IS_APPLE)
    set(VULKAN_LIB_PATH $ENV{HOME}/lib/VulkanSDK/1.3.290.1/macOS/lib/libvulkan.1.3.290.dylib)
    set(MOLTENVK_LIB_PATH $ENV{HOME}/lib/VulkanSDK/1.3.290.1/macOS/lib/libMoltenVK.dylib)
    set(FREETYPE_INCLUDE_DIRS "/opt/homebrew/include/freetype2")
    set(FREETYPE_LIBRARIES "/opt/homebrew/lib/libfreetype.dylib")
    # hard-coded path for openal-soft
    set(OPENAL_ROOT_DIR "/opt/homebrew/opt/openal-soft")
    set(OPENAL_INCLUDE_DIR "${OPENAL_ROOT_DIR}/include")
    set(OPENAL_LIBRARY "${OPENAL_ROOT_DIR}/lib/libopenal.dylib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
    include_directories(~/lib/VulkanSDK/1.3.290.1/macOS/include)
    target_link_libraries(${PROJECT_NAME} 
        ${VULKAN_LIB_PATH}
        ${MOLTENVK_LIB_PATH}
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_MACOS_MVK)
endif() # IS_APPLE

if(IS_LINUX)
    find_package(PkgConfig REQUIRED)
    find_package(X11 REQUIRED)
    pkg_check_modules(DRM REQUIRED libdrm)
    pkg_check_modules(SNDFILE REQUIRED sndfile)
    include_directories(${DRM_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME}
        ${X11_LIBRARIES}
        ${DRM_LIBRARIES}
        Xrandr
    )
endif() # IS_LINUX

if (IS_WINDOWS)
    # Vulkan
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib/vulkan-1.lib")
    set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include")
    include_directories(${Vulkan_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)

    # DirectX
    find_package(dxsdk-d3dx CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} Microsoft::D3DX9 Microsoft::D3DX10 Microsoft::D3DX11)

    if(MSVC)
        add_compile_options(/W4 /wd4100 /wd4201)
    endif()
endif() # IS_WINDOWS

find_package(Vulkan REQUIRED)
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

# For MS builds, group files so that they show up in hierarrchical order in VS solution explorer
if (IS_WINDOWS)
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
    foreach(fileItem ${HEADER_FILES})       
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
endif()
