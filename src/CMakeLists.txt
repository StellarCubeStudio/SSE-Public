cmake_minimum_required(VERSION 3.20)
project(StellarSeaEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND)
    find_package(glfw3 REQUIRED)
endif()

set(IMGUI_DIR src/libs/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

add_executable(VulkanEngine
    src/main.cpp
    src/Engine.cpp
    src/Engine.h
    ${IMGUI_SOURCES}
)

target_include_directories(VulkanEngine PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(VulkanEngine PRIVATE
    Vulkan::Vulkan
    glfw
)

target_compile_definitions(VulkanEngine PRIVATE
    VULKAN_HPP_RAII_NAMESPACE=raii
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    VK_USE_PLATFORM_WIN32_KHR
)

if(WIN32)
    target_compile_definitions(VulkanEngine PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
    target_compile_definitions(VulkanEngine PRIVATE VK_USE_PLATFORM_MACOS_MVK)
else()
    target_compile_definitions(VulkanEngine PRIVATE VK_USE_PLATFORM_XLIB_KHR)
    target_link_libraries(VulkanEngine PRIVATE dl pthread)
endif()