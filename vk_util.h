#pragma once

#include <vulkan/vulkan_core.h>


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
 * gl_FragCoord.x = (ndc.x + 1) * (vp.width  / 2) + vp.x;
 * gl_FragCoord.y = (ndc.y + 1) * (vp.height / 2) + vp.y; // Direc3D uses -ndc.y instead of ndc.y
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
