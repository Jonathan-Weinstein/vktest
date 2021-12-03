#include "vk_util.h"
#include "volk/volk.h"

#include "stb/stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct uvec2 { uint32_t x, y; };
struct uvec3 { uint32_t x, y, z; };

static void
#ifdef __GNUC__
__attribute__((noreturn))
#endif
VerifyVkResultFaild(VkResult r, const char *expr, int line)
{
   fprintf(stderr, "%s:%d (%s) returned non-VK_SUCCESS: %d\n", __FILE__, line, expr, r);
   exit(r);
}
#define VERIFY_VK(e) do { if (VkResult _r = e) VerifyVkResultFaild(_r, #e, __LINE__); } while(0)

static VkShaderModule
CreateShaderModule(VkDevice device, uint32_t nBytes, const void *spirv)
{
    VkShaderModule module = nullptr;
    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = nBytes;
    info.pCode = static_cast<const uint32_t *>(spirv);
    VERIFY_VK(vkCreateShaderModule(device, &info, VKU_ALLOC_CBS, &module));
    return module;
}

template<uint32_t N> static VkShaderModule
CreateShaderModule(VkDevice device, const uint32_t (&a)[N])
{
    return CreateShaderModule(device, N * sizeof (uint32_t), a);
}


static VkResult
CreateComputePipeline(VkDevice device,
                      VkShaderModule shaderModule,
                      VkPipelineLayout layout,
                      VkPipeline *pPipeline)  // OUT
{
    VkResult r;
    *pPipeline = VK_NULL_HANDLE;

    VkComputePipelineCreateInfo pipelineInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr, // pNext
        0, // VkPipelineCreateFlags
        {   VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0, // VkPipelineShaderStageCreateFlags
            VK_SHADER_STAGE_COMPUTE_BIT,
            shaderModule,
            "main",
            nullptr }, // pSpecializationInfo
        layout,
        VK_NULL_HANDLE, // basePipelineHandle
        -1, // basePipelineIndex
    };
    VERIFY_VK((r = vkCreateComputePipelines(device,
                                            VK_NULL_HANDLE, // VkPipelineCache
                                            1, &pipelineInfo,
                                            VKU_ALLOC_CBS,
                                            pPipeline)));
    return r;
}


// see uav_load_oob.comp:
static const uint32_t CsSpirvWords[] =
#include "ld_srv_typed_2darray.comp.h"
;

static void
CreatePipelineObjects(VkDevice device, bool bInputUav, VkPipeline *pPso, VkPipelineLayout *pPsoLayout, VkDescriptorSetLayout *pDescSetLayout)
{
    VkShaderModule shaderModule;
    {
        const uint32_t *pFinalCode = CsSpirvWords;
        uint32_t nCodeBytes = sizeof CsSpirvWords;
        uint32_t tmpcode[sizeof(CsSpirvWords) / sizeof(CsSpirvWords[0])];
        if (bInputUav) {
            const size_t InputOpTypeImageWordIndex = 0x000001f8 / 4;
            const size_t OpImageFetchWordIndex = 0x0000045c / 4;

            // Copy through fist 5 words of OpImageFetch { opcode, result type, result, image, coord }:
            memcpy(tmpcode, CsSpirvWords, (OpImageFetchWordIndex + 5) * sizeof(uint32_t));

            tmpcode[InputOpTypeImageWordIndex + 7] = 2; // "sampled"=2 means UAV
            tmpcode[InputOpTypeImageWordIndex + 8] = 33; // R32ui, replace Unknown

            tmpcode[OpImageFetchWordIndex + 0] = 5u << 16 | 98; // OpImageRead, replace OpImageFetch

            const size_t srcSuffixWordIndex = OpImageFetchWordIndex + 7; // skip image operands mask and lod=0
            memcpy(tmpcode + OpImageFetchWordIndex + 5,
                   CsSpirvWords + srcSuffixWordIndex,
                   sizeof(CsSpirvWords) - srcSuffixWordIndex * sizeof(uint32_t));
            pFinalCode = tmpcode;
            nCodeBytes = sizeof(CsSpirvWords) - 2 * sizeof(uint32_t);
        }
        shaderModule = CreateShaderModule(device, nCodeBytes, pFinalCode);
    }

    {
        VkDescriptorType inputDescriptorType = bInputUav ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        VkDescriptorSetLayoutBinding bindings[2] = {
            { 0, inputDescriptorType,              1, VK_SHADER_STAGE_COMPUTE_BIT },
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT }
        };
        VkDescriptorSetLayoutCreateInfo setInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            2, bindings
        };
        vkCreateDescriptorSetLayout(device, &setInfo, VKU_ALLOC_CBS, pDescSetLayout);
    }

    {
        const VkPushConstantRange pcRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, 16 };
        VkPipelineLayoutCreateInfo layoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1, pDescSetLayout,
            1, &pcRange
        };
        vkCreatePipelineLayout(device, &layoutInfo, VKU_ALLOC_CBS, pPsoLayout);
    }

    VERIFY_VK(CreateComputePipeline(device, shaderModule, *pPsoLayout, pPso));

    vkDestroyShaderModule(device, shaderModule, VKU_ALLOC_CBS);
}

#include "thirdparty/renderdoc_app.h"
extern RENDERDOC_API_1_1_2 *rdoc_api;

bool TestUavLoadOob(VkDevice device, VkQueue queue, uint32_t graphicsFamilyIndex, const VkPhysicalDeviceMemoryProperties& memProps)
{
    VkCommandPool cmdpool = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
    {
        const VkCommandPoolCreateInfo cmdPoolInfo = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
            0, // flags
            graphicsFamilyIndex
        };
        VERIFY_VK(vkCreateCommandPool(device, &cmdPoolInfo, VKU_ALLOC_CBS, &cmdpool));

        const VkCommandBufferAllocateInfo cmdBufAllocInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdpool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1 // commandBufferCount
        };
        VERIFY_VK(vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &cmdbuf));
    }

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    {
        static const VkDescriptorPoolSize poolSizes[] = {
           { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32 },
           { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32 },
        };
        const VkDescriptorPoolCreateInfo descPoolInfo = {
           VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr, // pNext
           0, // flags
           64,
           2, poolSizes
        };
        VERIFY_VK(vkCreateDescriptorPool(device, &descPoolInfo, VKU_ALLOC_CBS, &descriptorPool));
    }

    static constexpr uint32_t ImageWidth = 8, ImageHeight = 8;
    static constexpr uint32_t SerializedByteSizePerImage = ImageWidth * ImageHeight * sizeof(uint32_t);

    static constexpr uint32_t BufferByteCapacity = SerializedByteSizePerImage * 4;
    VkuBufferAndMemory stage;
    VERIFY_VK(vkuDedicatedBuffer(device, BufferByteCapacity, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &stage,
                                 memProps,
                                 VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    static const uint8_t ImageLayerCounts[5] = { 1, 3, 4, 5, 1 };
    static const struct Span { uint8_t base, n; } ViewLayerSpans[4] = { {0, 1}, {1, 1}, {0, 4}, {2, 2} };

    VkuImageAndMemory images[5]; // [4] is output image
    {
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = { ImageWidth, ImageHeight, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        for (int i = 0; i < 5; ++i) {
            imageInfo.format = i < 4 ? VK_FORMAT_R32_UINT : VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.flags = i < 4 ? 0 : VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            imageInfo.arrayLayers = ImageLayerCounts[i];
            VERIFY_VK(vkuDedicatedImage(device, imageInfo, &images[i], memProps));
        }
    }

    VkImageView views[5]; // [4] is output UAV, rest are 2D-array input UAV/SRV
    VkImageViewCreateInfo viewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        images[4].image,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_FORMAT_R32_UINT,
        { }, // VkComponentMapping all zeroes is identity
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VERIFY_VK(vkCreateImageView(device, &viewCreateInfo, VKU_ALLOC_CBS, &views[4]));
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; // for the rest

    VkMappedMemoryRange const MapRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, stage.memory, 0x0, VK_WHOLE_SIZE };
    void *pMap = nullptr;
    VERIFY_VK(vkMapMemory(device, stage.memory, 0, VK_WHOLE_SIZE, 0, &pMap));

    auto CmdClearLayers = [cmdbuf](VkImage image, Span span, uint32_t val) {
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, span.base, span.n };
        VkClearColorValue clearVal; for (uint32_t &r : clearVal.uint32) r = val;
        vkCmdClearColorImage(cmdbuf, image, VK_IMAGE_LAYOUT_GENERAL, &clearVal, 1, &range);
    };

    bool bPassed = true;

    bool const bUseDebugtUtil = true;
    if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
    for (int n = 2; n; --n) {
        bool const bUav = (n == 1);
        VkPipeline pso = VK_NULL_HANDLE;
        VkPipelineLayout psoLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;

        CreatePipelineObjects(device, bUav, &pso, &psoLayout, &descSetLayout);

        for (int i = 0; i < 4; ++i) {
            viewCreateInfo.image = images[i].image;
            viewCreateInfo.subresourceRange.baseArrayLayer = ViewLayerSpans[i].base;
            viewCreateInfo.subresourceRange.layerCount = ViewLayerSpans[i].n;
            VERIFY_VK(vkCreateImageView(device, &viewCreateInfo, VKU_ALLOC_CBS, &views[i]));
        }

        const uint32_t ColorOfLayer[5] = { 0xff0000ffu, 0xff00ff00u, 0xffff0000u, 0xff00ffffu, 0xffff00ffu };

        vkResetCommandPool(device, cmdpool, 0x0); // record commands:
        {
            const VkCommandBufferBeginInfo cmdBufbeginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
            };
            VERIFY_VK(vkBeginCommandBuffer(cmdbuf, &cmdBufbeginInfo));
            if (bUseDebugtUtil) {
                vkuCmdLabel(vkCmdBeginDebugUtilsLabelEXT, cmdbuf,
                            bUav ? "Input type = UAV" : "Input type = SRV",
                            bUav ? 0xffff0000 : 0xff00ff00);
            }

            /* For the input images, should only have to do this and the clears once: */
            VkImageMemoryBarrier ib[5] = { };
            for (int i = 0; i < 5; ++i) {
                ib[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                ib[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
                ib[i].image = images[i].image;
                ib[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                ib[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                ib[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                ib[i].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, -1u, 0, -1u };
            }
            vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                                 0, nullptr, 0, nullptr, 5, ib);
            for (int imageIndex = 0; imageIndex < 4; ++imageIndex) {
                for (int layer = 0; layer < ImageLayerCounts[imageIndex]; ++layer) {
                    CmdClearLayers(images[imageIndex].image, { uint8_t(layer), 1 }, ColorOfLayer[layer]);
                }
            }

            VkMemoryBarrier memBarrier = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            };
            vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0x0,
                                 1, &memBarrier, 0, nullptr, 0 , nullptr);
            const int32_t pcData[4] = { -1, 42, 0, 0 };
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pso);
            vkCmdPushConstants(cmdbuf, psoLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, pcData);
            VkDescriptorSet descSets[4] = { };
            VkDescriptorSetLayout descSetLayouts[4] = { descSetLayout, descSetLayout, descSetLayout, descSetLayout };
            VkDescriptorSetAllocateInfo descAllocInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                descriptorPool, 4, descSetLayouts
            };
            vkAllocateDescriptorSets(device, &descAllocInfo, descSets);
            for (unsigned inputImageIndex = 0;;) {
                VkDescriptorSet descSet = descSets[inputImageIndex];
                VkDescriptorImageInfo inputInfo = { VkSampler(), views[inputImageIndex], VK_IMAGE_LAYOUT_GENERAL };
                VkDescriptorImageInfo outputInfo = { VkSampler(), views[4], VK_IMAGE_LAYOUT_GENERAL };
                VkDescriptorType inputDescriptorType = bUav ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                VkWriteDescriptorSet writes[2] = {
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descSet, 0, 0, 1, // binding, arrayIndex, count
                      inputDescriptorType, &inputInfo, nullptr, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descSet, 1, 0, 1, // binding, arrayIndex, count
                      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outputInfo, nullptr, nullptr },
                };
                vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
                vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, psoLayout, 0, 1, &descSet, 0, nullptr);
                vkCmdDispatch(cmdbuf, 1, 1, 1);
                memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                                     1, &memBarrier, 0, nullptr, 0 , nullptr);
                VkBufferImageCopy bufImgCopy = { };
                bufImgCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; // mip, layer{begin, count}
                bufImgCopy.imageExtent = { ImageWidth, ImageHeight, 1 };
                bufImgCopy.bufferOffset = inputImageIndex * SerializedByteSizePerImage;
                bufImgCopy.bufferRowLength = ImageWidth;
                bufImgCopy.bufferImageHeight = ImageHeight;
                vkCmdCopyImageToBuffer(cmdbuf, images[4].image, VK_IMAGE_LAYOUT_GENERAL, stage.buffer, 1, &bufImgCopy);
                if (++inputImageIndex >= 4) {
                    break;
                }
                /* Wait for copy from output image to finish before clearing output image: */
                memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                                     1, &memBarrier, 0, nullptr, 0 , nullptr);
                CmdClearLayers(images[4].image, {0, 1}, 0xff7f7f7fu); // clear output to opaque gray
                /* Wait for clear to finish before write to output image via CS: */
                memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0x0,
                                     1, &memBarrier, 0, nullptr, 0 , nullptr);
            }
            memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            memBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0x0,
                                 1, &memBarrier, 0, nullptr, 0 , nullptr);
            if (bUseDebugtUtil) {
                vkCmdEndDebugUtilsLabelEXT(cmdbuf);
            }
            VERIFY_VK(vkEndCommandBuffer(cmdbuf));
        }

        // submit and WFI:
        {
            memset(pMap, 0xff, BufferByteCapacity); // opaque white
            vkFlushMappedMemoryRanges(device, 1, &MapRange);
            VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdbuf;
            VERIFY_VK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
            VERIFY_VK(vkQueueWaitIdle(queue));
            vkInvalidateMappedMemoryRanges(device, 1, &MapRange);
        }

        // inspect results:
        for (unsigned imageIndex = 0; imageIndex < 4; ++imageIndex) {
            const uint32_t *const pBaseU32 = (const uint32_t *)(SerializedByteSizePerImage*imageIndex + (const char *)pMap);
            bool bThisImagePass = true;
            for (uint y = 0; y < ImageHeight; ++y) {
                int numMismatchesThisRow = 0;
                for (uint x = 0; x < ImageWidth; ++x) {
                    // mostly copied from the shader:
                    uvec2 tid = { x, y };
                    uvec3 c;
                    c.x = tid.x + 1; // rightmost goes OOB
                    c.y = tid.y;
                    c.z = tid.y < 7u ? tid.y : uint32_t(-1);
                    Span const viewLayers = ViewLayerSpans[imageIndex];
                    uint32_t const expectedTexelValue =
                            (c.x >= ImageWidth ||
                             c.y >= ImageHeight ||
                             c.z >= viewLayers.n) ? 0 : ColorOfLayer[viewLayers.base + c.z];
                    uint32_t const gotTexelValue = pBaseU32[y * ImageWidth + x];
                    if (expectedTexelValue != gotTexelValue) {
                        bThisImagePass = false;
                        if (numMismatchesThisRow++ == 0) {
                            printf("Mismatch at x=%d, y=%d, %sV_%d, c={%d,%d,%d}: got=0x%08X, expected=0x%08X\n",
                                    x, y, bUav ? "UA" : "SR", imageIndex, c.x, c.y, c.z, gotTexelValue, expectedTexelValue);
                        }
                    }
                }
                if (numMismatchesThisRow >= 2) {
                    printf("%d more mismatches this row not reported\n", numMismatchesThisRow - 1);
                }
            }
            if (!bThisImagePass) {
                bPassed = false;
                char nameBuf[256];
                sprintf(nameBuf, "ld_%sv_typed_generated_%02d.png", bUav ? "ua" : "sr", imageIndex);
                printf("Saving failed result as %s\n", nameBuf);
                stbi_write_png(nameBuf, ImageWidth, ImageHeight, 4, pBaseU32, ImageWidth * sizeof(uint32_t));
                printf("Expected result is ld_typed_ref_%02d.png\n\n", imageIndex); // same for SRV and UAV
            }
        }

        for (int i = 0; i < 4; ++i) vkDestroyImageView(device, views[i], VKU_ALLOC_CBS); // keep output view
        vkDestroyPipeline(device, pso, VKU_ALLOC_CBS);
        vkDestroyPipelineLayout(device, psoLayout, VKU_ALLOC_CBS);
        vkDestroyDescriptorSetLayout(device, descSetLayout, VKU_ALLOC_CBS);
    }
    if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);

    vkDestroyDescriptorPool(device, descriptorPool, VKU_ALLOC_CBS);
    vkDestroyImageView(device, views[4], VKU_ALLOC_CBS);
    for (const VkuImageAndMemory& r : images) vkuDestroyImageAndFreeMemory(device, r);
    vkuDestroyBufferAndFreeMemory(device, stage);
    vkFreeCommandBuffers(device, cmdpool, 1, &cmdbuf);
    vkDestroyCommandPool(device, cmdpool, VKU_ALLOC_CBS);
    return bPassed;
}
