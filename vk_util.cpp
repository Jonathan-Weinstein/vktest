#ifndef VK_NO_PROTOTYPES
#error "Compile with -DVK_NO_PROTOTYPES"
#endif

#include "vk_util.h"
#include "volk/volk.h"

#define VKU_ALLOC_CBS static_cast<const VkAllocationCallbacks *>(nullptr)

static int
FindMemoryType(const VkPhysicalDeviceMemoryProperties& memProps,
               uint32_t requiredTypeBits,
               VkMemoryPropertyFlags desiredMemPropFlags)
{
    int atLeastIndex = -1;
    const unsigned nTypes = memProps.memoryTypeCount;
    for (unsigned i = 0; i < nTypes; ++i) {
        if (requiredTypeBits & (1u << i)) {
            const VkMemoryPropertyFlags flags = memProps.memoryTypes[i].propertyFlags;
            if (flags == desiredMemPropFlags) {
                return int(i); // first exact index
            } else if (atLeastIndex < 0 &&
                       (flags & desiredMemPropFlags) == desiredMemPropFlags) {
                atLeastIndex = int(i); // first at least index
            }
        }
    }
    return atLeastIndex;
}


// Vulkan-Docs missing .memoryRequirements and has weird non-ascii characters:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_KHR_dedicated_allocation.html

VkResult
vkuDedicatedImage(VkDevice device,
                  const VkImageCreateInfo &info,
                  VkuImageAndMemory *p,
                  const VkPhysicalDeviceMemoryProperties& memProps,
                  VkMemoryPropertyFlags memPropFlags)
{
    p->memory = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(device, &info, VKU_ALLOC_CBS, &p->image);
    if (result == VK_SUCCESS) {
        VkMemoryDedicatedRequirements dedicatedReqs = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr };
        VkMemoryRequirements2 reqs2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicatedReqs };
        VkImageMemoryRequirementsInfo2 imageReqs2 = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, p->image };
        vkGetImageMemoryRequirements2(device, &imageReqs2, &reqs2);
        int const sMemTypeIndex = FindMemoryType(memProps, reqs2.memoryRequirements.memoryTypeBits, memPropFlags);
        if (sMemTypeIndex >= 0) {
            VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {
                VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr,
                p->image, VkBuffer()
            };
            VkMemoryAllocateInfo allocInfo = {
                VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &dedicatedAllocInfo,
                reqs2.memoryRequirements.size, uint32_t(sMemTypeIndex)
            };
            result = vkAllocateMemory(device, &allocInfo, VKU_ALLOC_CBS, &p->memory);
            if (result == VK_SUCCESS) {
                result = vkBindImageMemory(device, p->image, p->memory, 0);
                if (result != VK_SUCCESS) {
                    vkFreeMemory(device, p->memory, VKU_ALLOC_CBS);
                    p->memory = VK_NULL_HANDLE;
                }
            }
        } else {
            result = VK_ERROR_UNKNOWN;
        }

        if (result != VK_SUCCESS) {
            vkDestroyImage(device, p->image, VKU_ALLOC_CBS);
            p->image = VK_NULL_HANDLE;
        }
    }
    return result;
}

void
vkuDestroyImageAndFreeMemory(VkDevice device, const VkuImageAndMemory& m)
{
    vkFreeMemory(device, m.memory, VKU_ALLOC_CBS);
    vkDestroyImage(device, m.image, VKU_ALLOC_CBS);
}
