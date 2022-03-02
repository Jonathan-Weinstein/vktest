#include "vk_simple_init.h"

#include "volk/volk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

template <class MainInfo, class Ext> void
PushFront(MainInfo *info, Ext *ext, VkStructureType sType)
{
    ext->sType = sType;
    ext->pNext = info->pNext;
    info->pNext = ext;
}

struct ExtensionPropsSet {
    const VkExtensionProperties *begin;
    const VkExtensionProperties *end;
};

static void
QueryDeviceExtProps(VkPhysicalDevice physDev,
                    ExtensionPropsSet *props) // OUT
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, nullptr);
    VkExtensionProperties *a = nullptr;
    if (count >= 1 && count < 2048) {
        a = (VkExtensionProperties *)malloc(count * sizeof *a); // 260 bytes per element
        vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, a);
    }
    props->begin = a;
    props->end = a + count;
}

static void
FreeExtProps(const ExtensionPropsSet *props) {
    free((void *)props->begin);
}

static bool
HasExtension(const ExtensionPropsSet &props, const char *extName) {
    const VkExtensionProperties *p = props.begin;
    const VkExtensionProperties *const pEnd = props.end;
    for (; p != pEnd; ++p) {
        if (strcmp(extName, p->extensionName) == 0) {
            return true;
        }
    }
    return false;
}

static const char *
GetDeviceTypeString(VkPhysicalDeviceType type) {
    static const char *const DeviceTypeNames[] = {
        "other",      // VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
        "integrated", // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1,
        "discrete",   // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2,
        "virtual",    // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU = 3,
        "CPU"         // VK_PHYSICAL_DEVICE_TYPE_CPU = 4
    };
    return size_t(type) < lengthof(DeviceTypeNames) ? DeviceTypeNames[type] : "???";
}

static VkBool32 VKAPI_PTR
DebugUtilsMessengerCallbackEXT(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *) // pUserData
{
    const char *severity;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severity = "ERROR";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severity = "WARNING";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        severity = "INFO";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        severity = "VERBOSE";
    else
        severity = "???";

    const char *type;
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        type = "PERFORMANCE";
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        type = "VALIDATION";
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        type = "GENERAL";
    else
        type = "???";

    fflush(stdout);
    fprintf(stderr, "DebugUtilsMessage: severity=%s, type=%s, {message}={%s}\n",
            severity, type, pCallbackData->pMessage);
    fflush(stderr);

    return VK_FALSE; // The application should always return VK_FALSE.
}

VkResult SimpleInitVulkan(VulkanObjetcs *vk, unsigned flags, int forceGpuIndex, GpuVendorID prefVendorID)
{
    enum : uint32_t { MinApiVersionNeeded = VK_API_VERSION_1_1 };

    *vk = { }; // zero
    vk->universalFamilyIndex = uint32_t(-1);

    // Create instance and set debug messenger if requested:
    {
        if (volkInitialize() != VK_SUCCESS ||
            volkGetInstanceVersion() < MinApiVersionNeeded) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkValidationFeaturesEXT validationFeaturesInfo = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
        VkValidationFeatureEnableEXT extraValidationEnables[] = { VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };

        VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.apiVersion = MinApiVersionNeeded;
        VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pApplicationInfo = &appInfo;

        const char *instanceExtensions[16];
        uint nInstanceExtensions = 0;
        const char *layers[16];
        uint nLayers = 0;

        bool const bValidate = (flags & (SIMPLE_INIT_VALIDATION_CORE | SIMPLE_INIT_VALIDATION_SYNC)) != 0;

        if (bValidate) {
            layers[nLayers++] = "VK_LAYER_KHRONOS_validation";

            instanceExtensions[nInstanceExtensions++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

            if (flags & SIMPLE_INIT_VALIDATION_SYNC) {
                instanceExtensions[nInstanceExtensions++] = VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;

                assert(createInfo.pNext == nullptr);
                validationFeaturesInfo.enabledValidationFeatureCount = 1;
                validationFeaturesInfo.pEnabledValidationFeatures = extraValidationEnables;
                createInfo.pNext = &validationFeaturesInfo;
            }
        }

        createInfo.ppEnabledLayerNames = nLayers ? layers : nullptr;
        createInfo.enabledLayerCount = nLayers;

        createInfo.ppEnabledExtensionNames = nInstanceExtensions ? instanceExtensions : nullptr;
        createInfo.enabledExtensionCount = nInstanceExtensions;

        for (uint i = 0; i < nLayers; ++i) {
            printf("Layer %s enabled.\n", layers[i]);
        }

        for (uint i = 0; i < nInstanceExtensions; ++i) {
            printf("Instance extension %s enabled.\n", instanceExtensions[i]);
        }

        VkResult res = vkCreateInstance(&createInfo, nullptr, &vk->instance);
        if (res == VK_SUCCESS) {
            volkLoadInstanceOnly(vk->instance);
            if (bValidate) {
                VkDebugUtilsMessengerCreateInfoEXT debugInfo = {
                    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                    nullptr,
                    0, // flags
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                        0,
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        // VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                        // VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                        0,
                    DebugUtilsMessengerCallbackEXT,
                    nullptr // pUserData
                };
                VkResult debugRes = vkCreateDebugUtilsMessengerEXT(
                    vk->instance, &debugInfo, nullptr, &vk->debugUtilsMessenger);
                if (debugRes != VK_SUCCESS) {
                    fprintf(stderr, "vkCreateDebugUtilsMessengerEXT returned %d\n", debugRes);
                    return debugRes;
                }
            }
        }
    }

    // Get physical device:
    {
        VkPhysicalDevice physicalDevices[16];
        uint32_t physicalDeviceCount = lengthof(physicalDevices);
        if (VkResult r = vkEnumeratePhysicalDevices(vk->instance, &physicalDeviceCount, physicalDevices)) {
            printf("vkEnumeratePhysicalDevices returned %d.\n", r);
            return r;
        }
        printf("physical device count: %d\n", physicalDeviceCount);

        int32_t bestScore = -1;
        int bestIndex = -1;
        for (int physDevIndex = 0; physDevIndex < int(physicalDeviceCount); ++physDevIndex) {
            VkPhysicalDevice const physdev = physicalDevices[physDevIndex];
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(physdev, &props);
            printf("VkPhysicalDevice[%d]: type=%s, apiVersion=0x%X, deviceName=%s, deviceID=%d, "
                   "vendorID=0x%X, driverVersion=0x%X\n",
                   physDevIndex, GetDeviceTypeString(props.deviceType), props.apiVersion,
                   props.deviceName, props.deviceID, props.vendorID,
                   props.driverVersion);

            int thisScore = 0;
            if (props.apiVersion < MinApiVersionNeeded) {
                printf("VkPhysicalDevice[%d] does not support version 1.1\n", physDevIndex);
                if (physDevIndex == forceGpuIndex) {
                    return VK_ERROR_FEATURE_NOT_PRESENT;
                }
            } else if (physDevIndex == forceGpuIndex) {
                thisScore = 0x7fffffff;
            } else {
                thisScore += (props.vendorID == uint32_t(prefVendorID)) ? 128 : 0;
            }

            if (thisScore > bestScore) {
                bestScore = thisScore;
                bestIndex = physDevIndex;
            }
        }
        if (bestIndex < 0) {
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        printf("Using VkPhysicalDevice[%d]\n", bestIndex);
        VkPhysicalDevice const physdev = physicalDevices[bestIndex];
        vk->physicalDevice = physdev;

        const char *deviceExtensions[16];
        int nDeviceExtensions = 0;
        {
            vk->props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            vk->features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

            ExtensionPropsSet extSet;
            QueryDeviceExtProps(physdev, &extSet);

            auto TestAndAppend = [&] (const char *name) -> bool {
                if (HasExtension(extSet, name)) {
                    deviceExtensions[nDeviceExtensions++] = name;
                    return true;
                }
                return false;
            };

            vk->NV_framebuffer_mixed_samples = TestAndAppend(VK_NV_FRAMEBUFFER_MIXED_SAMPLES_EXTENSION_NAME);
            vk->KHR_shader_draw_parameters = TestAndAppend(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
            vk->KHR_shader_float_controls = TestAndAppend(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

            if (TestAndAppend(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) {
                // no features
                PushFront(&vk->props2, &vk->pushDescriptorProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR);
            }

            if (TestAndAppend(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME)) {
                PushFront(&vk->features2, &vk->dynamicStateFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT);
                // no properties
            }


            if (TestAndAppend(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME)) {
                PushFront(&vk->features2, &vk->lineRasterizationFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT);
                // properties not useful
            }

            if (TestAndAppend(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME)) {
                PushFront(&vk->features2, &vk->xfbFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);
                PushFront(&vk->props2, &vk->xfbProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT);
            }

            if (flags & (SIMPLE_INIT_BUFFER_ROBUSTNESS_2 | SIMPLE_INIT_IMAGE_ROBUSTNESS_2 | SIMPLE_INIT_NULL_DESCRIPTOR)) {
                if (TestAndAppend(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
                    PushFront(&vk->features2, &vk->robustness2Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT);
                    // no properties
                }
            }

            FreeExtProps(&extSet);
        }

        vkGetPhysicalDeviceMemoryProperties(physdev, &vk->memProps);

        vkGetPhysicalDeviceFeatures2(physdev, &vk->features2);
        vkGetPhysicalDeviceProperties2(physdev, &vk->props2);

        /* Just enable everything that's supported (by passing pack the same structures filled by
         * vkGetPhysicalDeviceFeatures2) since most probably don't have downsides.
         * robustness might have a perf issue though (larger descriptors), so only enable that
         * if requeseted in flags.
         */
        if (!(flags & (SIMPLE_INIT_BUFFER_ROBUSTNESS_1 | SIMPLE_INIT_BUFFER_ROBUSTNESS_2))) {
            vk->features2.features.robustBufferAccess = false;
        }
        vk->robustness2Features.robustBufferAccess2 &= VkBool32((flags & SIMPLE_INIT_BUFFER_ROBUSTNESS_2) != 0);
        vk->robustness2Features.robustImageAccess2  &= VkBool32((flags & SIMPLE_INIT_IMAGE_ROBUSTNESS_2) != 0);
        vk->robustness2Features.nullDescriptor      &= VkBool32((flags & SIMPLE_INIT_NULL_DESCRIPTOR) != 0);

        // Find universal family:
        int sUniversalFamily = -1;
        {
            VkQueueFamilyProperties familyProps[32];
            uint32_t numFamilies = lengthof(familyProps);
            vkGetPhysicalDeviceQueueFamilyProperties(physdev, &numFamilies, familyProps);
            assert(numFamilies < 32u);
            assert(numFamilies);
            for (uint32_t fam = 0; fam < numFamilies; ++fam) {
                constexpr VkQueueFlags universalFlags = VK_QUEUE_GRAPHICS_BIT |
                                                        VK_QUEUE_COMPUTE_BIT |
                                                        VK_QUEUE_TRANSFER_BIT;
                if ((familyProps[fam].queueFlags & universalFlags) == universalFlags &&
                    sUniversalFamily < 0) {
                    sUniversalFamily = int(fam);
                }
            }
            if (sUniversalFamily < 0) {
                puts("no universal queue family found");
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }
        vk->universalFamilyIndex = uint(sUniversalFamily);

        // Create device:
        {
            const float queuePriorities[] = { 1.0f };

            VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            queueInfo.queueFamilyIndex = uint(sUniversalFamily);
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = queuePriorities;

            VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            createInfo.queueCreateInfoCount = 1;
            createInfo.pQueueCreateInfos = &queueInfo;

            createInfo.ppEnabledExtensionNames = deviceExtensions;
            createInfo.enabledExtensionCount = unsigned(nDeviceExtensions);

            for (int i = 0; i < nDeviceExtensions; ++i) {
                printf("Device extension %s enabled.\n", deviceExtensions[i]);
            }

            createInfo.pNext = &vk->features2;
            // If the pNext chain includes a VkPhysicalDeviceFeatures2 structure, then pEnabledFeatures must be NULL

            VkResult res = vkCreateDevice(physdev, &createInfo, ALLOC_CBS, &vk->device);
            if (res == VK_SUCCESS) {
                volkLoadDevice(vk->device);
                vkGetDeviceQueue(vk->device, uint(sUniversalFamily), 0, &vk->universalQueue);
            }
            return res;
        }
    }
}

void
SimpleDestroyVulkan(VulkanObjetcs *vk)
{
    if (vk->device) {
        vkDeviceWaitIdle(vk->device);
        vkDestroyDevice(vk->device, ALLOC_CBS);
    }
    if (vk->instance) {
        if (vk->debugUtilsMessenger) {
            vkDestroyDebugUtilsMessengerEXT(vk->instance, vk->debugUtilsMessenger, ALLOC_CBS);
        }
        vkDestroyInstance(vk->instance, ALLOC_CBS);
    }
}

