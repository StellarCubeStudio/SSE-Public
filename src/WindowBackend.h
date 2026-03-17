#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <vector>

class IWindowBackend {
public:
    virtual ~IWindowBackend() = default;
    virtual bool initialize(const std::string& title, int width, int height) = 0;
    virtual bool shouldClose() const = 0;
    virtual void pollEvents() = 0;
    virtual void cleanup() = 0;
    virtual std::vector<const char*> getRequiredInstanceExtensions() const = 0;
    virtual VkSurfaceKHR createSurface(VkInstance instance, VkAllocationCallbacks* allocator) = 0;
};