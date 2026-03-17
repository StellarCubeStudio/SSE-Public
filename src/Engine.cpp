#include "Engine.h"
#include <limits>
#include <algorithm>
#include <cstdlib>

// Static Callback
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->m_framebufferResized = true;
        app->m_width = width;
        app->m_height = height;
    }
}

Engine::Engine() {}

Engine::~Engine() {
    cleanup();
}

bool Engine::initialize() {
    if (!createWindow()) return false;
    if (!createVulkanInstance()) return false;
    if (!createSurface()) return false;
    if (!pickPhysicalDevice()) return false;
    if (!createLogicalDevice()) return false;
    if (!createSwapChain()) return false;
    if (!createImageViews()) return false;
    if (!createRenderPass()) return false;
    if (!createFramebuffers()) return false;
    if (!createCommandPool()) return false;
    if (!allocateCommandBuffers()) return false;
    if (!createSyncObjects()) return false;
    
    initImGui();

    LOG_INFO("Engine initialized successfully.");
    return true;
}

void Engine::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        drawFrame();
    }
    waitIdle();
}

void Engine::cleanup() {
    if (m_device) {
        vkDeviceWaitIdle(static_cast<VkDevice>(m_device));
        if (m_imguiDescriptorPool) {
            static_cast<VkDevice>(m_device).destroyDescriptorPool(m_imguiDescriptorPool);
        }
        ImGui_ImplVulkan_Shutdown();
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Sync Objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_imageAvailableSemaphores[i]) static_cast<VkDevice>(m_device).destroySemaphore(m_imageAvailableSemaphores[i]);
        if (m_renderFinishedSemaphores[i]) static_cast<VkDevice>(m_device).destroySemaphore(m_renderFinishedSemaphores[i]);
        if (m_inFlightFences[i]) static_cast<VkDevice>(m_device).destroyFence(m_inFlightFences[i]);
    }

    if (m_commandPool) static_cast<VkDevice>(m_device).destroyCommandPool(m_commandPool);
    
    m_swapchainFramebuffers.clear(); // RAII vector clear

    if (m_renderPass) static_cast<VkDevice>(m_device).destroyRenderPass(m_renderPass);

    m_swapchainImageViews.clear(); // RAII vector clear

    if (m_swapchain) static_cast<VkDevice>(m_device).destroySwapchainKHR(m_swapchain);
    
    // Clear Window
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void Engine::waitIdle() {
    if (m_device) {
        static_cast<VkDevice>(m_device).waitIdle();
    }
}

// Init

bool Engine::createWindow() {
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_width, m_height, "Vulkan Engine 2026 (C++23 + ImGui)", nullptr, nullptr);
    if (!m_window) {
        LOG_ERROR("Failed to create GLFW window");
        return false;
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);

    return true;
}

bool Engine::checkValidationLayerSupport() {
    auto layers = vk::enumerateInstanceLayerProperties();
    for (const auto& layer : layers) {
        if (strcmp(layer.layerName, m_validationLayers[0]) == 0) return true;
    }
    return false;
}

std::vector<const char*> Engine::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (m_enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

bool Engine::createVulkanInstance() {
    if (m_enableValidationLayers && !checkValidationLayerSupport()) {
        LOG_ERROR("Validation layers requested but not available!");
        return false;
    }

    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName = "Vulkan Engine 2026";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CustomEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    try {
        m_instance = vk::raii::Instance(createInfo);
        return true;
    } catch (const vk::SystemError& err) {
        LOG_ERROR("Failed to create Instance: " << err.what());
        return false;
    }
}

bool Engine::createSurface() {
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(m_instance), m_window, nullptr, &rawSurface) != VK_SUCCESS) {
        return false;
    }
    m_surface = vk::raii::SurfaceKHR(m_instance, rawSurface);
    return true;
}

bool Engine::pickPhysicalDevice() {
    auto devices = m_instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        LOG_ERROR("No GPUs with Vulkan support found!");
        return false;
    }

    for (auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_physicalDevice = std::move(device);
            LOG_INFO("Selected GPU: " << m_physicalDevice.getProperties().deviceName);
            return true;
        }
    }
    LOG_ERROR("No suitable GPU found!");
    return false;
}

bool Engine::isDeviceSuitable(vk::raii::PhysicalDevice& device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        auto formats = device.getSurfaceFormatsKHR(m_surface);
        auto modes = device.getSurfacePresentModesKHR(m_surface);
        swapChainAdequate = !formats.empty() && !modes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool Engine::checkDeviceExtensionSupport(vk::raii::PhysicalDevice& device) {
    auto availableExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

    for (const auto& ext : availableExtensions) {
        requiredExtensions.erase(ext.extensionName);
    }
    return requiredExtensions.empty();
}

Engine::QueueFamilyIndices Engine::findQueueFamilies(vk::raii::PhysicalDevice& device) {
    QueueFamilyIndices indices;
    auto families = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& family : families) {
        if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        device.getPhysicalDevice().getSurfaceSupportKHR(i, m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.isComplete()) break;
        i++;
    }
    return indices;
}

bool Engine::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo qInfo{};
        qInfo.queueFamilyIndex = family;
        qInfo.queueCount = 1;
        qInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(qInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures{};
    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    if (m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    try {
        m_device = m_physicalDevice.createDevice(createInfo);
        m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.value(), 0);
        m_presentQueue = m_device.getQueue(indices.presentFamily.value(), 0);
        return true;
    } catch (const vk::SystemError& err) {
        LOG_ERROR("Failed to create Logical Device: " << err.what());
        return false;
    }
}

vk::SurfaceFormatKHR Engine::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& fmt : availableFormats) {
        if (fmt.format == vk::Format::eB8G8R8A8Srgb && fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return fmt;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR Engine::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availableModes) {
    for (const auto& mode : availableModes) {
        if (mode == vk::PresentModeKHR::eMailbox) return mode; // Low latency triple buffering
    }
    return vk::PresentModeKHR::eFifo; // V-Sync
}

vk::Extent2D Engine::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    vk::Extent2D extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

bool Engine::createSwapChain() {
    auto support = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
    auto formats = m_physicalDevice.getSurfaceFormatsKHR(m_surface);
    auto modes = m_physicalDevice.getSurfacePresentModesKHR(m_surface);

    if (formats.empty() || modes.empty()) return false;

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(modes);
    vk::Extent2D extent = chooseSwapExtent(support);

    uint32_t imageCount = support.minImageCount + 1;
    if (support.maxImageCount > 0 && imageCount > support.maxImageCount) {
        imageCount = support.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = static_cast<VkSurfaceKHR>(m_surface);
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    createInfo.preTransform = support.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = nullptr;

    try {
        m_swapchain = m_device.createSwapchainKHR(createInfo);
        m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapchain);
        m_swapchainImageFormat = surfaceFormat.format;
        m_swapchainExtent = extent;
        return true;
    } catch (const vk::SystemError& err) {
        LOG_ERROR("Swapchain creation failed: " << err.what());
        return false;
    }
}

bool Engine::createImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    vk::ImageViewCreateInfo createInfo{};
    createInfo.viewType = vk::ImageViewType::e2D;
    createInfo.format = m_swapchainImageFormat;
    createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    for (size_t i = 0; i < m_swapchainImages.size(); i++) {
        createInfo.image = m_swapchainImages[i];
        m_swapchainImageViews[i] = m_device.createImageView(createInfo);
    }
    return true;
}

bool Engine::createRenderPass() {
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eNone;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo createInfo{};
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    try {
        m_renderPass = m_device.createRenderPass(createInfo);
        return true;
    } catch (const vk::SystemError& err) {
        LOG_ERROR("RenderPass creation failed: " << err.what());
        return false;
    }
}

bool Engine::createFramebuffers() {
    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
    vk::FramebufferCreateInfo createInfo{};
    createInfo.renderPass = m_renderPass;
    createInfo.attachmentCount = 1;
    createInfo.width = m_swapchainExtent.width;
    createInfo.height = m_swapchainExtent.height;
    createInfo.layers = 1;

    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        createInfo.pAttachments = reinterpret_cast<const VkImageView*>(&m_swapchainImageViews[i]);
        m_swapchainFramebuffers[i] = m_device.createFramebuffer(createInfo);
    }
    return true;
}

bool Engine::createCommandPool() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    vk::CommandPoolCreateInfo createInfo{};
    createInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    createInfo.queueFamilyIndex = indices.graphicsFamily.value();

    try {
        m_commandPool = m_device.createCommandPool(createInfo);
        return true;
    } catch (const vk::SystemError& err) {
        LOG_ERROR("CommandPool creation failed: " << err.what());
        return false;
    }
}

bool Engine::allocateCommandBuffers() {
    m_commandBuffers.resize(m_swapchainFramebuffers.size());
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    try {
        m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);
        return true;
    } catch (const vk::SystemError& err) {
        LOG_ERROR("CommandBuffer allocation failed: " << err.what());
        return false;
    }
}

bool Engine::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled; // Start signaled

    try {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_imageAvailableSemaphores[i] = m_device.createSemaphore(semInfo);
            m_renderFinishedSemaphores[i] = m_device.createSemaphore(semInfo);
            m_inFlightFences[i] = m_device.createFence(fenceInfo);
        }
        return true;
    } catch (const vk::SystemError& err) {
        LOG_ERROR("Sync objects creation failed: " << err.what());
        return false;
    }
}

void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = static_cast<VkInstance>(m_instance);
    init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(m_physicalDevice);
    init_info.Device = static_cast<VkDevice>(m_device);
    init_info.QueueFamily = findQueueFamilies(m_physicalDevice).graphicsFamily.value();
    init_info.Queue = m_graphicsQueue;
    init_info.PipelineCache = nullptr;
    init_info.DescriptorPool = nullptr;
    init_info.RenderPass = m_renderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    // Create Descriptor Pool
    vk::DescriptorPoolSize pool_sizes[] = {
        {vk::DescriptorType::eSampler, 1000},
        {vk::DescriptorType::eCombinedImageSampler, 1000},
        {vk::DescriptorType::eSampledImage, 1000},
        {vk::DescriptorType::eStorageImage, 1000},
        {vk::DescriptorType::eUniformTexelBuffer, 1000},
        {vk::DescriptorType::eStorageTexelBuffer, 1000},
        {vk::DescriptorType::eUniformBuffer, 1000},
        {vk::DescriptorType::eStorageBuffer, 1000},
        {vk::DescriptorType::eCombinedImageSampler, 1000},
        {vk::DescriptorType::eInputAttachment, 1000}
    };
    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    m_imguiDescriptorPool = static_cast<VkDescriptorPool>(m_device.createDescriptorPool(pool_info));
    init_info.DescriptorPool = m_imguiDescriptorPool;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

// Rend range

void Engine::recreateSwapChain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(m_window, &w, &h);
        glfwWaitEvents();
    }
    m_width = w;
    m_height = h;

    waitIdle(); // Wait before recreating

    // Cleanup old resources
    m_swapchainFramebuffers.clear();
    m_swapchainImageViews.clear();
    if (m_swapchain) {
        static_cast<VkDevice>(m_device).destroySwapchainKHR(m_swapchain);
        m_swapchain = nullptr;
    }

    // Recreate
    createSwapChain();
    createImageViews();
    createFramebuffers();
    // Note: RenderPass and CommandPool usually don't need recreation unless format changes
}

void Engine::drawFrame() {
    // 1. Wait for previous frame
    vk::Result waitResult = static_cast<VkDevice>(m_device).waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    if (waitResult != vk::Result::eSuccess) {
        LOG_ERROR("Wait for fence failed");
        return;
    }
    static_cast<VkDevice>(m_device).resetFences(1, &m_inFlightFences[m_currentFrame]);

    // 2. Acquire Image
    uint32_t imageIndex;
    vk::Result acquireResult = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], nullptr, &imageIndex);
    
    if (acquireResult == vk::Result::eOutOfDateKHR) {
        recreateSwapChain();
        return;
    } else if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR) {
        LOG_ERROR("Failed to acquire swap chain image!");
        return;
    }

    // 3. Record Command Buffer
    vk::CommandBuffer cmdBuf = m_commandBuffers[imageIndex];
    cmdBuf.reset(vk::CommandBufferResetFlagBits::eReleaseResources);

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmdBuf.begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = static_cast<VkFramebuffer>(m_swapchainFramebuffers[imageIndex]);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchainExtent;

    vk::ClearValue clearColor{{{0.1f, 0.1f, 0.15f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    cmdBuf.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    
    // --- ImGui Rendering ---
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    // Important: Scale coordinates for HiDPI displays if needed
    // drawData->ScaleClipRects(io.DisplayFramebufferScale); 
    ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuf);

    cmdBuf.endRenderPass();
    cmdBuf.end();

    // 4. Submit
    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
    submitInfo.pWaitDstStageMask = reinterpret_cast<const VkPipelineStageFlags*>(&vk::PipelineStageFlagBits::eColorAttachmentOutput);
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

    m_graphicsQueue.submit(submitInfo, m_inFlightFences[m_currentFrame]);

    // 5. Present
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vk::Result presentResult = m_presentQueue.presentKHR(&presentInfo);

    if (presentResult == vk::Result::eOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapChain();
    } else if (presentResult != vk::Result::eSuccess) {
        LOG_ERROR("Failed to present!");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}