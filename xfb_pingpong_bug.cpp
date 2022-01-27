#include "vk_simple_init.h"
#include "volk/volk.h"
#include "vk_util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "stb/stb_image_write.h"

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
    VERIFY_VK(vkCreateShaderModule(device, &info, ALLOC_CBS, &module));
    return module;
}

static VkShaderModule
CreateShaderModuleFromFile(VkDevice device, const char *path)
{
    if (FILE *const fp = fopen(path, "rb")) {
        long err = 0;
        err |= fseek(fp, 0, SEEK_END);
        long const size = ftell(fp);
        err |= size;
        err |= fseek(fp, 0, SEEK_SET);
        if (err < 0) {
            perror("fseek/ftell/fseek");
            exit(int(err));
        }
        if (size > 0x7fffffff || size < 20 || (size & 3u)) {
            printf("bad size = %ld\n", size);
            exit(42);
        }
        void *const data = malloc(size);
        if (fread(data, 1, size, fp) != size_t(size)) {
            perror("fread");
            exit(43);
        }
        VkShaderModule sm = CreateShaderModule(device, uint32_t(size), data);
        free(data);
        fclose(fp);
        return sm;
    } else {
        fflush(stdout);
        perror("fopen");
        fprintf(stderr, "(path)=(%s)\n", path);
        exit(1);
    }
}

struct Vertex {
   float position[4];
};

static const Vertex Verts[] = {
   { { -1.0f, -0.5f, 0.5f, 1.0f } },
   { { -0.5f,  0.5f, 0.5f, 1.0f } },
   { {  0.0f, -0.5f, 0.5f, 1.0f } },
   { {  0.5f,  0.5f, 0.5f, 1.0f } },
   { {  1.0f, -0.5f, 0.5f, 1.0f } }
};


// nullness of fs controls rasterizerDiscard and primitive topology:
static void
CreateGraphicsPipeline(VkDevice device,
                       VkShaderModule vs, VkShaderModule fs,
                       VkRenderPass renderpass, VkPipelineLayout pipelineLayout,
                       VkPipeline *pPipline)
{
    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    uint numStages = 0;
    VkPipelineShaderStageCreateInfo stages[4];
    auto PutStage = [&numStages, &stages](VkShaderStageFlagBits stageEnum, VkShaderModule sm) {
        if (sm) {
            stages[numStages++] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, stageEnum, sm, "main" };
        }
    };
    PutStage(VK_SHADER_STAGE_VERTEX_BIT, vs);
    PutStage(VK_SHADER_STAGE_FRAGMENT_BIT, fs);

    static const VkDynamicState DynamicStateList[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    static const VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0x0,
        lengthof(DynamicStateList), DynamicStateList
    };

    VkPipelineColorBlendAttachmentState attBlendWriteAll = { };
    attBlendWriteAll.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo blendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blendState.attachmentCount = 1;
    blendState.pAttachments = &attBlendWriteAll;

    static const VkPipelineDepthStencilStateCreateInfo NoDepthStencil = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    const VkPipelineMultisampleStateCreateInfo msaaInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0x0,
        VK_SAMPLE_COUNT_1_BIT
    };

    static const VkPipelineViewportStateCreateInfo SingleViewportInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0x0,
        1, nullptr,
        1, nullptr
    };

    const VkPipelineInputAssemblyStateCreateInfo InputAssemblyPrimInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0x0,
        fs ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    };

    const VkVertexInputBindingDescription vbBinding0 = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    const VkVertexInputAttributeDescription attrib0 = { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 };
    static const VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0x0,
        1, &vbBinding0,
        1, &attrib0
    };

    VkPipelineRasterizationStateCreateInfo rast = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_NONE;
    rast.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rast.lineWidth = 1.0f;
    rast.rasterizerDiscardEnable = !fs;

    info.basePipelineHandle = VK_NULL_HANDLE;
    info.basePipelineIndex = -1;
    info.layout = pipelineLayout;
    info.renderPass = renderpass;
    info.subpass = 0;

    info.pTessellationState = nullptr;
    info.pDynamicState = &DynamicStateInfo;
    info.pViewportState = &SingleViewportInfo;
    info.pColorBlendState = &blendState;
    info.pDepthStencilState = &NoDepthStencil;
    info.pMultisampleState = &msaaInfo;
    info.pInputAssemblyState = &InputAssemblyPrimInfo;
    info.pVertexInputState = &VertexInputInfo;
    info.pRasterizationState = &rast;

    info.stageCount = numStages;
    info.pStages = stages;

    VERIFY_VK(vkCreateGraphicsPipelines(device, VkPipelineCache(), 1, &info, ALLOC_CBS, pPipline));
}


bool TestXfbPingPong(const VulkanObjetcs& vk)
{
    VkDevice const device = vk.device;
    const VkExtent3D ImageSize = { 512, 512, 1 };
    const VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM;

    VkCommandPool cmdpool = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
    {
        const VkCommandPoolCreateInfo cmdPoolInfo = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
            0, // flags
            vk.universalFamilyIndex
        };
        VERIFY_VK(vkCreateCommandPool(device, &cmdPoolInfo, ALLOC_CBS, &cmdpool));

        const VkCommandBufferAllocateInfo cmdBufAllocInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdpool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1 // commandBufferCount
        };
        VERIFY_VK(vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &cmdbuf));
    }

    VkuBufferAndMemory stage;
    const uint32_t PackedImageByteSize = ImageSize.width * ImageSize.height * sizeof(uint32_t);
    const uint32_t StageByteCapacity = PackedImageByteSize;
    vkuDedicatedBuffer(device, StageByteCapacity, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &stage, vk.memProps,
                       VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    VkuBufferAndMemory buffers[2] = { };
    {
        void *pMapMem[2] = { };
        for (unsigned i = 0; i < lengthof(buffers); ++i) {
            vkuDedicatedBuffer(device, sizeof Verts,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT,
                &buffers[i], vk.memProps,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            VERIFY_VK(vkMapMemory(device, buffers[i].memory, 0, VK_WHOLE_SIZE, 0, &pMapMem[i]));
        }
        memset(pMapMem[0], 0, sizeof Verts);
        memcpy(pMapMem[1], Verts, sizeof Verts);
        for (const auto& r : buffers) { vkUnmapMemory(device, r.memory); }
    }

    VkuBufferAndMemory xfbCounter;
    vkuDedicatedBuffer(device, sizeof Verts,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT,
        &xfbCounter, vk.memProps,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkuImageAndMemory outputImag;
    {
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = Format;
        imageInfo.extent = ImageSize;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        vkuDedicatedImage(device, imageInfo, &outputImag, vk.memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    // ------------------------------------------------------------

    VkRenderPass renderpass;
    {
        VkAttachmentDescription attDesc = { };
        attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attDesc.format = Format;
        attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        attDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpassDesc = { };
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &attRef;
        VkRenderPassCreateInfo renderpassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0,
            1, &attDesc, 1, &subpassDesc,
        };
        VERIFY_VK(vkCreateRenderPass(device, &renderpassInfo, ALLOC_CBS, &renderpass));
    }
    VkImageView outputView;
    {
        VkImageViewCreateInfo viewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0, // VkImageViewCreateFlags
            outputImag.image,
            VK_IMAGE_VIEW_TYPE_2D,
            Format,
            VkComponentMapping(), // identity
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // single mip, single layer
        };
        VERIFY_VK(vkCreateImageView(device, &viewCreateInfo, ALLOC_CBS, &outputView));
    }
    VkFramebuffer framebuffer;
    {
        VkFramebufferCreateInfo framebufferInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0x0,
            renderpass,
            1, &outputView,
            ImageSize.width, ImageSize.height, 1
        };
        VERIFY_VK(vkCreateFramebuffer(device, &framebufferInfo, ALLOC_CBS, &framebuffer));
    }

    // ------------------------------------------------------------

    const VkExtent2D EmptyFramebufferSize = {
        vk.props2.properties.limits.maxFramebufferWidth,
        vk.props2.properties.limits.maxFramebufferHeight
    };
    VkRenderPass emptyRenderpass;
    {
        VkSubpassDescription subpassDesc = { };
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        VkRenderPassCreateInfo renderpassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0x0,
            0, nullptr, 1, &subpassDesc,
        };
        VERIFY_VK(vkCreateRenderPass(device, &renderpassInfo, ALLOC_CBS, &emptyRenderpass));
    }
    VkFramebuffer emptyFramebuffer;
    {
        VkFramebufferCreateInfo framebufferInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0x0,
            emptyRenderpass,
            0, nullptr,
            EmptyFramebufferSize.width, EmptyFramebufferSize.height, 1
        };
        VERIFY_VK(vkCreateFramebuffer(device, &framebufferInfo, ALLOC_CBS, &emptyFramebuffer));
    }
    const VkRenderPassBeginInfo EmptyFramebufferBeginRenderpassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr,
        emptyRenderpass, emptyFramebuffer, { {0,0}, EmptyFramebufferSize }
    };

    // ------------------------------------------------------------

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    {
        // no descriptors or push constants:
        VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        VERIFY_VK(vkCreatePipelineLayout(device, &info, ALLOC_CBS, &pipelineLayout));
    }

    VkShaderModule vs_xfb = CreateShaderModuleFromFile(device, "shaders/vs_xfb.spv");
    VkShaderModule vs_plain = CreateShaderModuleFromFile(device, "shaders/vs_plain.spv");
    VkShaderModule fs = CreateShaderModuleFromFile(device, "shaders/fs.spv");
    VkPipeline pso_xfb = VK_NULL_HANDLE;
    VkPipeline pso_rast = VK_NULL_HANDLE;
    CreateGraphicsPipeline(device, vs_xfb, VK_NULL_HANDLE, emptyRenderpass, pipelineLayout, &pso_xfb);
    CreateGraphicsPipeline(device, vs_xfb, fs, renderpass, pipelineLayout, &pso_rast);

    const VkRect2D RenderArea = {
        { 0, 0 }, { ImageSize.width, ImageSize.height }
    };
    // begin cmdbuf:
    {
        const VkCommandBufferBeginInfo cmdBufbeginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
        };
        VERIFY_VK(vkBeginCommandBuffer(cmdbuf, &cmdBufbeginInfo));
        VkViewport vp;
        vkuUpwardsViewportFromRect(RenderArea, 0, 1, &vp);
        vkCmdSetViewport(cmdbuf, 0, 1, &vp);
        vkCmdSetScissor(cmdbuf, 0, 1, &RenderArea);
    }

    static const VkDeviceSize WholeSizes[1] = { VK_WHOLE_SIZE };
    static const VkDeviceSize ZeroOffsets[1] = { 0 };

    vkuCmdLabel(vkCmdBeginDebugUtilsLabelEXT, cmdbuf, "XFB loop", 0xff0000ffu);
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pso_xfb);
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &buffers[1].buffer, ZeroOffsets);
    for (int i = 0; i < 511; ++i) { // XXX
        vkCmdBeginRenderPass(cmdbuf, &EmptyFramebufferBeginRenderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindTransformFeedbackBuffersEXT(cmdbuf, 0, 1, &buffers[i & 1].buffer, ZeroOffsets, WholeSizes); // XXX: pass dummy buffer too, and all 4?
        vkCmdBeginTransformFeedbackEXT(cmdbuf, 0, 1, nullptr, nullptr);
        vkCmdDraw(cmdbuf, 5, 1, 0, 0);
        vkCmdEndTransformFeedbackEXT(cmdbuf, 0, 1, &xfbCounter.buffer, ZeroOffsets);
        vkCmdEndRenderPass(cmdbuf);
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &buffers[i & 1].buffer, ZeroOffsets);
        VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
            // src, keep only write bits, but still do execution barrier for VertexInput
            VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |
            VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
            // dst
            (i == 1022 ? 0u : VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |
                              VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT) |
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        };
        vkCmdPipelineBarrier(cmdbuf,
                             VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, //
                             (i == 1022 ? 0u : VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT) | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0x0,
                             1, &barrier, 0, nullptr, 0 , nullptr);
    }
    vkCmdEndDebugUtilsLabelEXT(cmdbuf);

    VkImageMemoryBarrier imageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
        0, // srcAccessMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        outputImag.image,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0x0,
                         0, nullptr, 0, nullptr, 1, &imageBarrier);

    const VkRenderPassBeginInfo rpBeginInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr,
        renderpass, framebuffer, RenderArea
    };
    vkCmdBeginRenderPass(cmdbuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        VkClearAttachment attClear = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, // attachment
            {{{ 0.0f, 0.0f, 0.5f, 1.0f }}}
        };
        VkClearRect clearRect = { RenderArea, 0, 1 }; // layer range
        vkCmdClearAttachments(cmdbuf, 1, &attClear, 1, &clearRect);
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pso_rast);
        vkCmdDraw(cmdbuf, 5, 1, 0, 0);
    }
    vkCmdEndRenderPass(cmdbuf);

    imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                         0, nullptr, 0, nullptr, 1, &imageBarrier);

    VkBufferImageCopy bufImgCopy = { };
    bufImgCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; // mip, layer{begin, count}
    bufImgCopy.imageExtent = ImageSize;
    bufImgCopy.bufferOffset = 0;
    bufImgCopy.bufferRowLength = ImageSize.width;
    bufImgCopy.bufferImageHeight = ImageSize.height;
    vkCmdCopyImageToBuffer(cmdbuf, outputImag.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stage.buffer, 1, &bufImgCopy);

    // end cmdbuf:
    {
        VkMemoryBarrier dev2hostBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_HOST_READ_BIT
        };
        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0x0,
                             1, &dev2hostBarrier, 0, nullptr, 0 , nullptr);
        VERIFY_VK(vkEndCommandBuffer(cmdbuf));
    }

    // submit:
    void *pMap = nullptr;
    {
        const VkDeviceSize offset = 0, size = StageByteCapacity;
        VERIFY_VK(vkMapMemory(device, stage.memory, offset, size, 0, &pMap));
        const VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, stage.memory, offset, size };
        memset(pMap, 0xCC, StageByteCapacity);
        vkFlushMappedMemoryRanges(device, 1, &range);
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        VERIFY_VK(vkQueueSubmit(vk.universalQueue, 1, &submitInfo, VK_NULL_HANDLE));
        VERIFY_VK(vkQueueWaitIdle(vk.universalQueue));
        vkInvalidateMappedMemoryRanges(device, 1, &range);
    }

    stbi_write_png("xfb_output.png", ImageSize.width, ImageSize.height, 4,
                   (const unsigned char *)pMap + 0*PackedImageByteSize, ImageSize.width*sizeof(uint32_t));

    vkDestroyPipeline(device, pso_xfb, ALLOC_CBS);
    vkDestroyPipeline(device, pso_rast, ALLOC_CBS);
    vkDestroyPipelineLayout(device, pipelineLayout, ALLOC_CBS);
    vkFreeCommandBuffers(device, cmdpool, 1, &cmdbuf);
    vkDestroyCommandPool(device, cmdpool, ALLOC_CBS);
    vkDestroyImageView(device, outputView, ALLOC_CBS);
    vkDestroyFramebuffer(device, framebuffer, ALLOC_CBS);
    vkDestroyFramebuffer(device, emptyFramebuffer, ALLOC_CBS);
    vkDestroyRenderPass(device, renderpass, ALLOC_CBS);
    vkDestroyRenderPass(device, emptyRenderpass, ALLOC_CBS);
    vkuDestroyImageAndFreeMemory(device, outputImag);
    vkuDestroyBufferAndFreeMemory(device, stage);
    vkuDestroyBufferAndFreeMemory(device, xfbCounter);
    for (auto& r : buffers) vkuDestroyBufferAndFreeMemory(device, r);

    vkDestroyShaderModule(device, vs_xfb, ALLOC_CBS);
    vkDestroyShaderModule(device, vs_plain, ALLOC_CBS);
    vkDestroyShaderModule(device, fs, ALLOC_CBS);

    fflush(stderr);
    printf("leaving %s\n", __FUNCTION__);
    fflush(stdout);
    return true;
}
