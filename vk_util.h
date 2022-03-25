#pragma once

#include <vulkan/vulkan_core.h>

#define VKU_ALLOC_CBS static_cast<const VkAllocationCallbacks *>(nullptr)

typedef unsigned uint;

/*
 * Both Direct3D and Vulkan use the same equation for clipspace X to
 * framebuffer X, but Direct3D use negates ndc.y for the Y coord.
 * This negation can be mimicked by modifying the viewport's offset and height parameters.
 *
 * Note that in both APIs, a triangle's winding is based ultimately on its framebuffer coordinates,
 * and both use the same convention for clockwise/ccw.
 *
 * ---
 *
 * vec3 ndc = gl_Position.xyz / gl_Position.w;
 *
 * vec2 RasterCoord; // may be quantized depending on subpixel bits:
 * RasterCoord.x = (ndc.x + 1) * (vp.width  / 2) + vp.x;
 * RasterCoord.y = (ndc.y + 1) * (vp.height / 2) + vp.y; // Direct3D uses -ndc.y instead of ndc.y
 *
 * For default gl_FragCoord interpolation modifier:
 *      gl_FragCoord.xy = floor(RasterCoord.xy) + vec2(0.5, 0.5);
 *
 * gl_FragCoord.z = ndc.z * (b - a) + a;
 */
inline void
vkuUpwardsViewportFromRect(const VkRect2D& r, float a, float b, VkViewport *vp)
{
    int32_t const y = r.offset.y;
    int32_t const h = int32_t(r.extent.height);
    vp->x        = float(r.offset.x);
    vp->y        = float(y + h);
    vp->width    = float(r.extent.width);
    vp->height   = float(-h);
    vp->minDepth = a;
    vp->maxDepth = b;
}

struct VkuImageAndMemory {
    VkImage image;
    VkDeviceMemory memory;
};

VkResult
vkuDedicatedImage(VkDevice device,
                  const VkImageCreateInfo &info,
                  VkuImageAndMemory *p,
                  const VkPhysicalDeviceMemoryProperties& memProps,
                  VkMemoryPropertyFlags memPropFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
void
vkuDestroyImageAndFreeMemory(VkDevice device, const VkuImageAndMemory& m);


struct VkuBufferAndMemory {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

VkResult
vkuDedicatedBuffer(VkDevice device,
                   VkDeviceSize bytesize,
                   VkBufferUsageFlags usageFlags,
                   VkuBufferAndMemory *p,
                   const VkPhysicalDeviceMemoryProperties& memProps,
                   VkMemoryPropertyFlags memPropFlags);
void
vkuDestroyBufferAndFreeMemory(VkDevice device, const VkuBufferAndMemory& m);

// pfn can be vkCmdBeginDebugUtilsLabelEXT or vkCmdInsertDebugUtilsLabelEXT
inline void
vkuCmdLabel(PFN_vkCmdBeginDebugUtilsLabelEXT pfn, VkCommandBuffer cmdbuf, const char *s, uint32_t color = 0)
{
    VkDebugUtilsLabelEXT info;
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pNext = nullptr;
    info.pLabelName = s;
    for (unsigned i = 0; i < 4; ++i) {
        info.color[i] = int((color >> (i*8)) & 0xff) / 255.0f;
    }
    pfn(cmdbuf, &info);
}


inline const char *
StringFromVkResult(VkResult result)
{
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    // case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
    // case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    // case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    // case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    default:
        return "???";
    }
}
