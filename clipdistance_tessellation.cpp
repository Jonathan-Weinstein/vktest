#include "vk_simple_init.h"
#include "volk/volk.h"
#include "vk_util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "stb/stb_image_write.h"


struct D3D11_QUERY_DATA_PIPELINE_STATISTICS {
  uint64_t IAVertices;
  uint64_t IAPrimitives;
  uint64_t VSInvocations;
  uint64_t GSInvocations;
  uint64_t GSPrimitives;
  uint64_t CInvocations;
  uint64_t CPrimitives;
  uint64_t PSInvocations;
  uint64_t HSInvocations;
  uint64_t DSInvocations;
  uint64_t CSInvocations;
};

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
        perror("fopen");
        fprintf(stderr, "(path)=(%s)\n", path);
        exit(1);
    }
}

static void
CreateGraphicsPipeline(VkDevice device,
                       VkShaderModule vs, VkShaderModule tesc, VkShaderModule tese, VkShaderModule fs,
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
    PutStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tesc);
    PutStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tese);
    PutStage(VK_SHADER_STAGE_FRAGMENT_BIT, fs);

    static const VkDynamicState DynamicStateList[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    static const VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr, 0,
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
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_SAMPLE_COUNT_1_BIT
    };

    static const VkPipelineViewportStateCreateInfo SingleViewportInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        nullptr, 0,
        1, nullptr,
        1, nullptr
    };

    const VkPipelineInputAssemblyStateCreateInfo InputAssemblyPrimInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr, 0,
        tese ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    const VkPipelineTessellationStateCreateInfo TessInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, nullptr, 0,
        3 // patchControlPoints
    };

    static const VkPipelineVertexInputStateCreateInfo NoVertexInputInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineRasterizationStateCreateInfo rast = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_NONE;
    rast.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rast.lineWidth = 1.0f;

    info.basePipelineHandle = VK_NULL_HANDLE;
    info.basePipelineIndex = -1;
    info.layout = pipelineLayout;
    info.renderPass = renderpass;
    info.subpass = 0;

    info.pTessellationState = tese ? &TessInfo : nullptr;
    info.pDynamicState = &DynamicStateInfo;
    info.pViewportState = &SingleViewportInfo;
    info.pColorBlendState = &blendState;
    info.pDepthStencilState = &NoDepthStencil;
    info.pMultisampleState = &msaaInfo;
    info.pInputAssemblyState = &InputAssemblyPrimInfo;
    info.pVertexInputState = &NoVertexInputInfo;
    info.pRasterizationState = &rast;

    info.stageCount = numStages;
    info.pStages = stages;

    VERIFY_VK(vkCreateGraphicsPipelines(device, VkPipelineCache(), 1, &info, ALLOC_CBS, pPipline));
}


bool TestClipDistanceIo(VkDevice device, VkQueue queue, uint32_t graphicsFamilyIndex, const VkPhysicalDeviceMemoryProperties& memProps)
{
    const VkExtent3D ImageSize = { 256, 256, 1 };
    const VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM;

    VkQueryPool pipelineStatsQueryPool = VK_NULL_HANDLE;
    {
        VkQueryPoolCreateInfo info = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            nullptr,
            0x0, // flags
            VK_QUERY_TYPE_PIPELINE_STATISTICS,
            2, // count
            /* 11 bits, matches full D3D11_QUERY_DATA_PIPELINE_STATISTICS: */
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT
        };
        VERIFY_VK(vkCreateQueryPool(device, &info, ALLOC_CBS, &pipelineStatsQueryPool));
    }

    VkCommandPool cmdpool = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
    {
        const VkCommandPoolCreateInfo cmdPoolInfo = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
            0, // flags
            graphicsFamilyIndex
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
    const uint32_t StageByteCapacity = 4 * PackedImageByteSize;
    vkuDedicatedBuffer(device, StageByteCapacity, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &stage, memProps,
                       VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);


    VkuImageAndMemory resources[2];
    for (VkuImageAndMemory& resource : resources) {
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = Format;
        imageInfo.extent = ImageSize;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        vkuDedicatedImage(device, imageInfo, &resource, memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    VkRenderPass renderpass;
    {
        VkAttachmentDescription attDesc = { };
        attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attDesc.format = Format;
        attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        attDesc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_GENERAL };
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

    VkImageView views[2];
    for (int i = 0; i < 2; ++i) {
        VkImageViewCreateInfo viewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0, // VkImageViewCreateFlags
            resources[i].image,
            VK_IMAGE_VIEW_TYPE_2D,
            Format,
            VkComponentMapping(), // identity
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // single mip, single layer
        };
        VERIFY_VK(vkCreateImageView(device, &viewCreateInfo, ALLOC_CBS, &views[i]));
    }

    VkFramebuffer framebuffers[2];
    for (int i = 0; i < 2; ++i){
        VkFramebufferCreateInfo framebufferInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0,
            renderpass,
            1, &views[i],
            ImageSize.width, ImageSize.height, 1
        };
        VERIFY_VK(vkCreateFramebuffer(device, &framebufferInfo, ALLOC_CBS, &framebuffers[i]));
    }


    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    {
        // no descriptors or push constants:
        VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        VERIFY_VK(vkCreatePipelineLayout(device, &info, ALLOC_CBS, &pipelineLayout));
    }

    VkPipeline pipelines[4] = { };
    {
        /*
         * cd build-vktest-Desktop-Debug
         * ln -s ../vktest/shaders shaders
         * ls shaders
         *      # hello.txt
         * ls -l shaders
         *      # shaders -> ../vktest/shaders
         */
        VkShaderModule vs_clipdist = CreateShaderModuleFromFile(device, "shaders/clipdist_vs.spv");
        VkShaderModule hs_clipdist = CreateShaderModuleFromFile(device, "shaders/clipdist_hs.spv");

        VkShaderModule vs_generic = CreateShaderModuleFromFile(device, "shaders/generic_vs.spv");
        VkShaderModule hs_generic = CreateShaderModuleFromFile(device, "shaders/generic_hs.spv");

        VkShaderModule ps = CreateShaderModuleFromFile(device, "shaders/clipdist_ps.spv");
        VkShaderModule ds = CreateShaderModuleFromFile(device, "shaders/clipdist_ds.spv");

        CreateGraphicsPipeline(device, vs_clipdist, VK_NULL_HANDLE, VK_NULL_HANDLE, ps, renderpass, pipelineLayout, &pipelines[0]);
        CreateGraphicsPipeline(device, vs_clipdist, hs_clipdist, ds, ps, renderpass, pipelineLayout, &pipelines[1]);

        CreateGraphicsPipeline(device, vs_generic, VK_NULL_HANDLE, VK_NULL_HANDLE, ps, renderpass, pipelineLayout, &pipelines[2]);
        CreateGraphicsPipeline(device, vs_generic, hs_generic, ds, ps, renderpass, pipelineLayout, &pipelines[3]);


        vkDestroyShaderModule(device, vs_clipdist, ALLOC_CBS);
        vkDestroyShaderModule(device, hs_clipdist, ALLOC_CBS);

        vkDestroyShaderModule(device, vs_generic, ALLOC_CBS);
        vkDestroyShaderModule(device, hs_generic, ALLOC_CBS);

        vkDestroyShaderModule(device, ps, ALLOC_CBS);
        vkDestroyShaderModule(device, ds, ALLOC_CBS);
    }

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

        vkCmdResetQueryPool(cmdbuf, pipelineStatsQueryPool, 0, 2); // first, count
    }

    for (int i = 0; i < 2; ++i) {
        vkuCmdLabel(vkCmdBeginDebugUtilsLabelEXT, cmdbuf,
                    i == 0 ? "vs->hs via ClipDistance" : "vs->hs via generic", 0xff000000u | 0xffu << (i*8));
        VkImageMemoryBarrier imageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
            0, // srcAccessMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resources[i].image,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(cmdbuf,
                             i == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT, // i>=1: wait for img->buf copy to finish
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0x0,
                             0, nullptr, 0, nullptr, 1, &imageBarrier);

        vkCmdBeginQuery(cmdbuf, pipelineStatsQueryPool, i, 0x0);
        {
            const VkRenderPassBeginInfo rpBeginInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr,
                renderpass, framebuffers[i], RenderArea
            };
            vkCmdBeginRenderPass(cmdbuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            {
                VkClearAttachment attClear;
                attClear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                attClear.colorAttachment = 0;
                attClear.clearValue.color.float32[0] = 1.0f;
                attClear.clearValue.color.float32[1] = 0.5f;
                attClear.clearValue.color.float32[2] = 0.125f;
                attClear.clearValue.color.float32[3] = 1.0f;
                VkClearRect clearRect = { RenderArea, 0, 1 }; // layer range
                vkCmdClearAttachments(cmdbuf, 1, &attClear, 1, &clearRect);

                vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[i*2 + 0]);
                vkCmdDraw(cmdbuf, 4*2*3, 1,     0, 0);
                vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[i*2 + 1]);
                vkCmdDraw(cmdbuf, 4*2*3, 1, 4*2*3, 0);
            }
            vkCmdEndRenderPass(cmdbuf);
        }
        vkCmdEndQuery(cmdbuf, pipelineStatsQueryPool, i);

        VkMemoryBarrier memBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        };
        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                             1, &memBarrier, 0, nullptr, 0 , nullptr);
        VkBufferImageCopy bufImgCopy = { };
        bufImgCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; // mip, layer{begin, count}
        bufImgCopy.imageExtent = ImageSize;
        bufImgCopy.bufferOffset = i * PackedImageByteSize;
        bufImgCopy.bufferRowLength = ImageSize.width;
        bufImgCopy.bufferImageHeight = ImageSize.height;
        vkCmdCopyImageToBuffer(cmdbuf, resources[i].image, VK_IMAGE_LAYOUT_GENERAL, stage.buffer, 1, &bufImgCopy);
        vkCmdEndDebugUtilsLabelEXT(cmdbuf);
    }

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
        VERIFY_VK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        VERIFY_VK(vkQueueWaitIdle(queue));
        vkInvalidateMappedMemoryRanges(device, 1, &range);

        D3D11_QUERY_DATA_PIPELINE_STATISTICS queryData[2] = { };
        vkGetQueryPoolResults(device, pipelineStatsQueryPool, 0, 2, // first, count
                              sizeof queryData, queryData, sizeof(queryData[0]),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        for (int i = 0; i < 2; ++i) {
            const D3D11_QUERY_DATA_PIPELINE_STATISTICS& r = queryData[i];
            printf("Relevant QueryResult[%d] = {\n", i);
            printf("   IAVertices    = %lu\n", r.IAVertices);
            printf("   IAPrimitives  = %lu\n", r.IAPrimitives);
            printf("   VSInvocations = %lu\n", r.VSInvocations);
            printf("   CInvocations  = %lu\n", r.CInvocations);
            printf("   CPrimitives   = %lu\n", r.CPrimitives);
            printf("   PSInvocations = %lu\n", r.PSInvocations);
            printf("   HSInvocations = %lu\n", r.HSInvocations);
            printf("   DSInvocations = %lu\n", r.DSInvocations);
            puts("}");
        }
    }

    bool bTestPassed = false;
    {
        stbi_write_png("pass_via_clipdist.png", ImageSize.width, ImageSize.height, 4,
                       (const unsigned char *)pMap + 0*PackedImageByteSize, ImageSize.width*sizeof(uint32_t));


        stbi_write_png("pass_via_generic.png", ImageSize.width, ImageSize.height, 4,
                       (const unsigned char *)pMap + 1*PackedImageByteSize, ImageSize.width*sizeof(uint32_t));
    }

    vkDestroyQueryPool(device, pipelineStatsQueryPool, ALLOC_CBS);
    for (auto pipeline : pipelines) vkDestroyPipeline(device, pipeline, ALLOC_CBS);
    vkDestroyPipelineLayout(device, pipelineLayout, ALLOC_CBS);
    vkFreeCommandBuffers(device, cmdpool, 1, &cmdbuf);
    vkDestroyCommandPool(device, cmdpool, ALLOC_CBS);
    for (auto view : views) vkDestroyImageView(device, view, ALLOC_CBS);
    for (auto framebuffer : framebuffers) vkDestroyFramebuffer(device, framebuffer, ALLOC_CBS);
    vkDestroyRenderPass(device, renderpass, ALLOC_CBS);
    for (const VkuImageAndMemory& resource : resources) vkuDestroyImageAndFreeMemory(device, resource);
    vkuDestroyBufferAndFreeMemory(device, stage);

    return bTestPassed;
}

// Welp, the bug from dx11-d2d-tessellation-tir doesnt repro like this.
// Also note that the DS uvw colorIds are different than NV.
// Might be missing Flat interp modifier or something about tess provoking vertex.
