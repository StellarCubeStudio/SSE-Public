cmake_minimum_required(VERSION 3.20)

project(StellarSeaEngine 
    VERSION 0.1.0 
    DESCRIPTION "StellarSeaEngine - A Vulkan based Game Engine"
    LANGUAGES CXX
)

# C++23 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) 

FetchContent_MakeAvailable(glfw)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")

if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR 
        "ImGui source not found at ${IMGUI_DIR}.\n"
        "Please run: git clone https://github.com/ocornut/imgui.git ${IMGUI_DIR}"
    )
endif()

message(STATUS "Using ImGui from: ${IMGUI_DIR}")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

add_executable(StellarSeaEngine
    src/main.cpp
    src/Engine.cpp
    src/Engine.h
    ${IMGUI_SOURCES}
)

target_include_directories(StellarSeaEngine PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(StellarSeaEngine PRIVATE
    Vulkan::Vulkan
    glfw
)

target_compile_definitions(StellarSeaEngine PRIVATE
    VULKAN_HPP_RAII_NAMESPACE=raii
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    ENGINE_NAME="StellarSeaEngine"
)

if(WIN32)
    target_compile_definitions(StellarSeaEngine PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
    target_compile_definitions(StellarSeaEngine PRIVATE VK_USE_PLATFORM_MACOS_MVK)
    target_link_libraries(StellarSeaEngine PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
else()
    target_compile_definitions(StellarSeaEngine PRIVATE VK_USE_PLATFORM_XLIB_KHR)
    target_link_libraries(StellarSeaEngine PRIVATE dl pthread)
endif()

set_target_properties(StellarSeaEngine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)