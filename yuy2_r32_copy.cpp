#include "vk_simple_init.h"
#include "volk/volk.h"
#include "vk_util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static void
#ifdef __GNUC__
__attribute__((noreturn))
#endif
VerifyVkResultFaild(VkResult r, const char *expr, int line)
{
   fprintf(stderr, "%s:%d (%s) returned non-VK_SUCCESS: %s\n", __FILE__, line, expr, StringFromVkResult(r));
   exit(r);
}
#define VERIFY_VK(e) do { if (VkResult _r = (e)) VerifyVkResultFaild(_r, #e, __LINE__); } while(0)


void TestYuy2Copy(const VulkanObjetcs& vk)
{
    VkuBufferAndMemory upload;
    VkuBufferAndMemory readback;
    VkuImageAndMemory yuy2;
    VkuImageAndMemory r32ui;

    constexpr int NumBlocksX = 128, NumBlocksY = NumBlocksX * 2;
    constexpr int BufferByteSize = NumBlocksX * NumBlocksY * sizeof(uint32_t);

    vkuDedicatedBuffer(vk.device, BufferByteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &upload, vk.memProps,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void *pUploadMap = nullptr;
    vkMapMemory(vk.device, upload.memory, 0, VK_WHOLE_SIZE, 0x0, &pUploadMap);

    vkuDedicatedBuffer(vk.device, BufferByteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &readback, vk.memProps,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    void *pReadbackMap = nullptr;
    vkMapMemory(vk.device, readback.memory, 0, VK_WHOLE_SIZE, 0x0, &pReadbackMap);

    for (uint32_t i = 0; i < NumBlocksX * NumBlocksY; ++i) {
        reinterpret_cast<uint32_t *>(pUploadMap)[i] = i;
    }
    memset(pReadbackMap, 0xCD, BufferByteSize);

    VkCommandPool cmdpool = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
    {
        const VkCommandPoolCreateInfo cmdPoolInfo = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
            0, // flags
            vk.universalFamilyIndex
        };
        VERIFY_VK(vkCreateCommandPool(vk.device, &cmdPoolInfo, ALLOC_CBS, &cmdpool));

        const VkCommandBufferAllocateInfo cmdBufAllocInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdpool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1 // commandBufferCount
        };
        VERIFY_VK(vkAllocateCommandBuffers(vk.device, &cmdBufAllocInfo, &cmdbuf));
    }


    {
        const VkCommandBufferBeginInfo cmdBufbeginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
        };
        VERIFY_VK(vkBeginCommandBuffer(cmdbuf, &cmdBufbeginInfo));
    }

    {
        VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_G8B8G8R8_422_UNORM; // YUY2
        info.extent = { NumBlocksX * 2, NumBlocksY * 1, 1 };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        vkuDedicatedImage(vk.device, info, &yuy2, vk.memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    {
        VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R32_UINT;
        info.extent = { NumBlocksX, NumBlocksY, 1 };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_STORAGE_BIT;
        vkuDedicatedImage(vk.device, info, &r32ui, vk.memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    VkImageMemoryBarrier imgbar = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
        0, // srcAccessMask,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        VkImage(),
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    imgbar.image = yuy2.image;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                         0, nullptr, 0, nullptr, 1, &imgbar);
    imgbar.image = r32ui.image;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                         0, nullptr, 0, nullptr, 1, &imgbar);
    /* 1: Init R32_UINT image: */
    VkBufferImageCopy bufImgCopy = { };
    bufImgCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    bufImgCopy.imageExtent = { NumBlocksX, NumBlocksY, 1 };
    bufImgCopy.bufferRowLength = NumBlocksX;
    bufImgCopy.bufferImageHeight = NumBlocksY;
    vkCmdCopyBufferToImage(cmdbuf, upload.buffer, r32ui.image, VK_IMAGE_LAYOUT_GENERAL, 1, &bufImgCopy);
    VkMemoryBarrier membar = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT
    };
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                         1, &membar, 0, nullptr, 0, nullptr);
    /* 2: Copy from R32_UINT image to YUY2 image; results in VK_ERROR_DEVICE_LOST on NV when waiting: */
    VkImageCopy imgCopy = { };
    imgCopy.extent = { NumBlocksX, NumBlocksY, 1 }; // use src (r32ui) pixel dims, so do not multiply NnumBlocksX by 2
    imgCopy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    imgCopy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    vkCmdCopyImage(cmdbuf, r32ui.image, VK_IMAGE_LAYOUT_GENERAL, yuy2.image, VK_IMAGE_LAYOUT_GENERAL, 1, &imgCopy);
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                         1, &membar, 0, nullptr, 0, nullptr);
    /* 3: Copy from YUY2 image to host-cached buffer: */
    bufImgCopy.imageExtent.width *= 2;
    bufImgCopy.bufferRowLength *= 2;
    vkCmdCopyImageToBuffer(cmdbuf, yuy2.image, VK_IMAGE_LAYOUT_GENERAL, readback.buffer, 1, &bufImgCopy);
    membar.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0x0,
                         1, &membar, 0, nullptr, 0, nullptr);

    fflush(stdout);
    fflush(stderr);
    {
        VERIFY_VK(vkEndCommandBuffer(cmdbuf));
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        VERIFY_VK(vkQueueSubmit(vk.universalQueue, 1, &submitInfo, VK_NULL_HANDLE));
        VERIFY_VK(vkDeviceWaitIdle(vk.device));
    }


    int nBlocksMismatch = 0;
    int i = 0;
    for (int y = 0; y < NumBlocksY; ++y) {
        for (int xblock = 0; xblock < NumBlocksX; ++xblock) {
            assert(i == y*NumBlocksX + xblock);
            uint32_t const val = reinterpret_cast<const uint32_t *>(pReadbackMap)[i];
            if (val != uint32_t(i)) {
                nBlocksMismatch++;
                printf("(%d, %d): got 0x%X, expected 0x%X\n", xblock, y, val, i);
            }
            i++;
        }
    }


    vkuDestroyImageAndFreeMemory(vk.device, r32ui);
    vkuDestroyImageAndFreeMemory(vk.device, yuy2);
    vkuDestroyBufferAndFreeMemory(vk.device, readback);
    vkuDestroyBufferAndFreeMemory(vk.device, upload);

    vkDestroyCommandPool(vk.device, cmdpool, ALLOC_CBS);

    if (nBlocksMismatch == 0) {
        puts("Test passed.");
    } else {
        printf("Test failed, nBlocksMismatch=%d.\n", nBlocksMismatch);
    }
}

