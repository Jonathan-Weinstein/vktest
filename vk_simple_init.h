#pragma once

#include <vulkan/vulkan_core.h>

#define ALLOC_CBS static_cast<const VkAllocationCallbacks *>(nullptr)

typedef unsigned uint;

template<class T, size_t N> char (&_lengthof_helper(T(&)[N]))[N];
#define lengthof(a) sizeof(_lengthof_helper(a))

struct VulkanObjetcs {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugUtilsMessenger;

    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkQueue universalQueue;
    uint32_t universalFamilyIndex;

    VkPhysicalDeviceMemoryProperties memProps;

    // --------------------------------------------

    bool NV_framebuffer_mixed_samples;
    bool KHR_shader_draw_parameters;
    bool KHR_shader_float_controls;

    VkPhysicalDeviceProperties2 props2;
    VkPhysicalDeviceFeatures2 features2;
    // VK_EXT_robustness2:
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features;
    // VK_EXT_transform_feedback:
    VkPhysicalDeviceTransformFeedbackFeaturesEXT xfbFeatures;
    VkPhysicalDeviceTransformFeedbackPropertiesEXT xfbProperties;
    // VK_KHR_push_descriptor:
    VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProperties;
    // VK_EXT_extended_dynamic_state:
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamicStateFeatures;
    // VK_EXT_line_rasterization:
    VkPhysicalDeviceLineRasterizationFeaturesEXT lineRasterizationFeatures;
    // VK_KHR_dynamic_rendering:
    // VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures;
};

enum : unsigned {
    SIMPLE_INIT_BUFFER_ROBUSTNESS_1 = 1 << 0,
    SIMPLE_INIT_BUFFER_ROBUSTNESS_2 = 1 << 1,
    SIMPLE_INIT_IMAGE_ROBUSTNESS_2  = 1 << 2,
    SIMPLE_INIT_NULL_DESCRIPTOR     = 1 << 3,
    SIMPLE_INIT_VALIDATION_CORE     = 1 << 4,
    SIMPLE_INIT_VALIDATION_SYNC     = 1 << 5,
};


enum class GpuVendorID : uint16_t {
    Unknown     =      0u,
    Intel       = 0x8086u,
    Nvidia      = 0x10DEu
};

// forceGpuIndex = -1 (or out of bounds) means no preference. Otherwise uses VkPhysicalDevice[gpuIndex]
VkResult SimpleInitVulkan(VulkanObjetcs *vk, unsigned flags, int forceGpuIndex = -1, GpuVendorID prefVendorID = GpuVendorID::Intel);


void SimpleDestroyVulkan(VulkanObjetcs *vk);

