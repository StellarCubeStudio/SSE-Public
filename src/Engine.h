#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <iostream>
#include <stdexcept>

#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl

class Engine {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool initialize();
    void run();
    void cleanup();

private:
    // Init
    bool createWindow();
    bool createVulkanInstance();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapChain();
    bool createImageViews();
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandPool();
    bool allocateCommandBuffers();
    bool createSyncObjects();
    void initImGui();

    // Runtime
    void drawFrame();
    void recreateSwapChain();
    void waitIdle();

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(vk::raii::PhysicalDevice& device);
    bool checkDeviceExtensionSupport(vk::raii::PhysicalDevice& device);
    
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };
    QueueFamilyIndices findQueueFamilies(vk::raii::PhysicalDevice& device);

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    // Member Values
    GLFWwindow* m_window = nullptr;
    int m_width = 800;
    int m_height = 600;
    bool m_framebufferResized = false;

    // Vulkan RAII Core
    vk::raii::Instance m_instance{nullptr};
    vk::raii::SurfaceKHR m_surface{nullptr};
    vk::raii::PhysicalDevice m_physicalDevice{nullptr};
    vk::raii::Device m_device{nullptr};
    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;

    // Swapchain & Views
    vk::SwapchainKHR m_swapchain = nullptr;
    std::vector<VkImage> m_swapchainImages;
    vk::Format m_swapchainImageFormat;
    vk::Extent2D m_swapchainExtent;
    std::vector<vk::raii::ImageView> m_swapchainImageViews;

    // Render Pass & Framebuffers
    vk::RenderPass m_renderPass = nullptr;
    std::vector<vk::raii::Framebuffer> m_swapchainFramebuffers;

    // Command Buffers
    vk::CommandPool m_commandPool = nullptr;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    // Sync Objects (Per Frame in Flight)
    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;
    uint32_t m_currentFrame = 0;

    // ImGui Resources
    vk::DescriptorPool m_imguiDescriptorPool = nullptr;

    // Config
    const std::vector<const char*> m_validationLayers = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*> m_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const bool m_enableValidationLayers = true;
};