cmake_minimum_required(VERSION 3.10)
project(StellarSeaEngine)
find_package(Vulkan REQUIRED)
add_executable(StellarSeaEngine main.cpp)
target_include_directories(StellarSeaEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(StellarSeaEngine Vulkan::Vulkan)
target_sources(StellarSeaEngine PRIVATE 
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_vulkan.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp
)
target_include_directories(StellarSeaEngine PRIVATE ${Vulkan_INCLUDE_DIRS})