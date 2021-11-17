#include "vk_simple_init.h"
#include "volk/volk.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "stb/stb_image_write.h"
#include "stb/stb_image.h"

struct vec2f { float x, y; };

struct PackedR8G8B8 {
    uint8_t r, g, b;
};

inline bool operator==(PackedR8G8B8 c0, PackedR8G8B8 c1)
{
    return c0.r == c1.r && c0.g == c1.g && c0.b == c1.b;
}

inline bool operator!=(PackedR8G8B8 c0, PackedR8G8B8 c1)
{
    return !(c0 == c1);
}


// glslc -O --target-env=vulkan1.1 xy01_attrib_passthru.vert -mfmt=c -o -
static const uint32_t VsSpirv[] =
{0x07230203,0x00010300,0x000d000a,0x0000001b,
0x00000000,0x00020011,0x00000001,0x0006000b,
0x00000001,0x4c534c47,0x6474732e,0x3035342e,
0x00000000,0x0003000e,0x00000000,0x00000001,
0x0007000f,0x00000000,0x00000004,0x6e69616d,
0x00000000,0x0000000d,0x00000012,0x00050048,
0x0000000b,0x00000000,0x0000000b,0x00000000,
0x00050048,0x0000000b,0x00000001,0x0000000b,
0x00000001,0x00050048,0x0000000b,0x00000002,
0x0000000b,0x00000003,0x00050048,0x0000000b,
0x00000003,0x0000000b,0x00000004,0x00030047,
0x0000000b,0x00000002,0x00040047,0x00000012,
0x0000001e,0x00000000,0x00020013,0x00000002,
0x00030021,0x00000003,0x00000002,0x00030016,
0x00000006,0x00000020,0x00040017,0x00000007,
0x00000006,0x00000004,0x00040015,0x00000008,
0x00000020,0x00000000,0x0004002b,0x00000008,
0x00000009,0x00000001,0x0004001c,0x0000000a,
0x00000006,0x00000009,0x0006001e,0x0000000b,
0x00000007,0x00000006,0x0000000a,0x0000000a,
0x00040020,0x0000000c,0x00000003,0x0000000b,
0x0004003b,0x0000000c,0x0000000d,0x00000003,
0x00040015,0x0000000e,0x00000020,0x00000001,
0x0004002b,0x0000000e,0x0000000f,0x00000000,
0x00040017,0x00000010,0x00000006,0x00000002,
0x00040020,0x00000011,0x00000001,0x00000010,
0x0004003b,0x00000011,0x00000012,0x00000001,
0x0004002b,0x00000006,0x00000014,0x00000000,
0x0004002b,0x00000006,0x00000015,0x3f800000,
0x00040020,0x00000019,0x00000003,0x00000007,
0x00050036,0x00000002,0x00000004,0x00000000,
0x00000003,0x000200f8,0x00000005,0x0004003d,
0x00000010,0x00000013,0x00000012,0x00050051,
0x00000006,0x00000016,0x00000013,0x00000000,
0x00050051,0x00000006,0x00000017,0x00000013,
0x00000001,0x00070050,0x00000007,0x00000018,
0x00000016,0x00000017,0x00000014,0x00000015,
0x00050041,0x00000019,0x0000001a,0x0000000d,
0x0000000f,0x0003003e,0x0000001a,0x00000018,
0x000100fd,0x00010038};


// glslc -O --target-env=vulkan1.1 write_sample_mask.frag -mfmt=c -o -
static const uint32_t FsSpirv[] =
{0x07230203,0x00010300,0x000d000a,0x00000013,
0x00000000,0x00020011,0x00000001,0x0006000b,
0x00000001,0x4c534c47,0x6474732e,0x3035342e,
0x00000000,0x0003000e,0x00000000,0x00000001,
0x0007000f,0x00000004,0x00000004,0x6e69616d,
0x00000000,0x00000008,0x0000000d,0x00030010,
0x00000004,0x00000007,0x00040047,0x00000008,
0x0000001e,0x00000000,0x00030047,0x0000000d,
0x0000000e,0x00040047,0x0000000d,0x0000000b,
0x00000014,0x00020013,0x00000002,0x00030021,
0x00000003,0x00000002,0x00040015,0x00000006,
0x00000020,0x00000000,0x00040020,0x00000007,
0x00000003,0x00000006,0x0004003b,0x00000007,
0x00000008,0x00000003,0x00040015,0x00000009,
0x00000020,0x00000001,0x0004002b,0x00000006,
0x0000000a,0x00000001,0x0004001c,0x0000000b,
0x00000009,0x0000000a,0x00040020,0x0000000c,
0x00000001,0x0000000b,0x0004003b,0x0000000c,
0x0000000d,0x00000001,0x0004002b,0x00000009,
0x0000000e,0x00000000,0x00040020,0x0000000f,
0x00000001,0x00000009,0x00050036,0x00000002,
0x00000004,0x00000000,0x00000003,0x000200f8,
0x00000005,0x00050041,0x0000000f,0x00000010,
0x0000000d,0x0000000e,0x0004003d,0x00000009,
0x00000011,0x00000010,0x0004007c,0x00000006,
0x00000012,0x00000011,0x0003003e,0x00000008,
0x00000012,0x000100fd,0x00010038};



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

template<uint32_t N> static VkShaderModule
CreateShaderModule(VkDevice device, const uint32_t (&a)[N])
{
    return CreateShaderModule(device, N * sizeof (uint32_t), a);
}

typedef struct BufferAndMemory {
    VkBuffer buffer;
    VkDeviceMemory memory;
} BufferAndMemory;

typedef struct ImageAndMemory {
    VkImage image;
    VkDeviceMemory memory;
} ImageAndMemory;

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

static void
CreateBufferAndMemory(VkDevice device,
                      const VkPhysicalDeviceMemoryProperties& memProps,
                      VkMemoryPropertyFlags desiredMemProps,
                      uint32_t bufferByteSize, VkBufferUsageFlags usage,
                      BufferAndMemory *p) {
    VkBufferCreateInfo bufInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size = bufferByteSize;
    bufInfo.usage = usage;
    VERIFY_VK(vkCreateBuffer(device, &bufInfo, ALLOC_CBS, &p->buffer));

    VkMemoryRequirements bufReqs;
    vkGetBufferMemoryRequirements(device, p->buffer, &bufReqs);
    const int sMemIndex = FindMemoryType(memProps, bufReqs.memoryTypeBits, desiredMemProps);
    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = bufReqs.size;
    allocInfo.memoryTypeIndex = uint32_t(sMemIndex);
    if (sMemIndex < 0) {
        puts("no good buffer mem type index");
        exit(2);
    }
    VERIFY_VK(vkAllocateMemory(device, &allocInfo, ALLOC_CBS, &p->memory));
    VERIFY_VK(vkBindBufferMemory(device, p->buffer, p->memory, 0));
}

static void
CreateImageAndMemory(VkDevice device,
                     const VkPhysicalDeviceMemoryProperties& memProps,
                     VkMemoryPropertyFlags desiredMemProps,
                     const VkImageCreateInfo& imageInfo,
                     ImageAndMemory *p) {
    VERIFY_VK(vkCreateImage(device, &imageInfo, ALLOC_CBS, &p->image));

    VkMemoryRequirements imageReqs;
    vkGetImageMemoryRequirements(device, p->image, &imageReqs);
    const int sMemIndex = FindMemoryType(memProps, imageReqs.memoryTypeBits, desiredMemProps);
    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = imageReqs.size;
    allocInfo.memoryTypeIndex = uint32_t(sMemIndex);
    if (sMemIndex < 0) {
        puts("no good image mem type index");
        exit(3);
    }
    VERIFY_VK(vkAllocateMemory(device, &allocInfo, ALLOC_CBS, &p->memory));
    VERIFY_VK(vkBindImageMemory(device, p->image, p->memory, 0));
}

static void
DestroyBufferAndFreeMemory(VkDevice device, BufferAndMemory a)
{
    vkDestroyBuffer(device, a.buffer, ALLOC_CBS);
    vkFreeMemory(device, a.memory, ALLOC_CBS);
}

static void
DestroyImageAndFreeMemory(VkDevice device, ImageAndMemory a)
{
    vkDestroyImage(device, a.image, ALLOC_CBS);
    vkFreeMemory(device, a.memory, ALLOC_CBS);
}

// Current mechanism in proposed extension to disambiguite from VK_AMD_mixed_attachment_samples:
#define VK_PIPELINE_MULTISAMPLE_STATE_CREATE_RASTER_MULTISAMPLE_BIT_EXT 0x00000001

static void
CreateExtRasterMultisamplePipeline(VkDevice device, VkShaderModule vs, VkShaderModule fs, VkRenderPass renderpass,
                                   VkPipelineLayout pipelineLayout, VkSampleCountFlagBits rasterSamples, VkPipeline *pPipline)
{
    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    const VkPipelineShaderStageCreateInfo stages[] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main" },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main" },
    };

    static const VkDynamicState DynamicStateList[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    static const VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr, 0,
        lengthof(DynamicStateList), DynamicStateList
    };

    // Set XOR logic op:
    VkPipelineColorBlendAttachmentState attBlendWriteAll = { };
    attBlendWriteAll.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo blendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blendState.logicOpEnable = VK_TRUE;
    blendState.logicOp = VK_LOGIC_OP_XOR;
    blendState.attachmentCount = 1;
    blendState.pAttachments = &attBlendWriteAll;

    static const VkPipelineDepthStencilStateCreateInfo NoDepthStencil = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    assert(uint32_t(rasterSamples) <= 32u && ((16 | 8 | 4 | 2) & rasterSamples) != 0);
    // VK_SAMPLE_COUNT_x_BIT == x for all x in {1,2,4,8,16,32,64}
    const VkSampleMask sampleMask = uint32_t(-1) >> (32 - rasterSamples); // set all bits in raster pattern
    const VkPipelineMultisampleStateCreateInfo msaaInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        VK_PIPELINE_MULTISAMPLE_STATE_CREATE_RASTER_MULTISAMPLE_BIT_EXT, // will cause validation error
        rasterSamples,
        false, // sampleShadingEnable
        0.0f,
        &sampleMask
    };

    static const VkPipelineViewportStateCreateInfo SingleViewportInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        nullptr, 0,
        1, nullptr,
        1, nullptr
    };

    static const VkPipelineInputAssemblyStateCreateInfo TriangleListNoRestart = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    static const VkVertexInputAttributeDescription Attribs[] = {
        // location, binding, format, offsetof in struct:
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
    };
    static const VkVertexInputBindingDescription InputSlot0Info= {
        // binding, stride, rate:
        0, sizeof(float)*2, VK_VERTEX_INPUT_RATE_VERTEX
    };
    static const VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr, 0,
        1, &InputSlot0Info,
        lengthof(Attribs), Attribs
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

    info.pTessellationState = nullptr;
    info.pDynamicState = &DynamicStateInfo;
    info.pViewportState = &SingleViewportInfo;
    info.pColorBlendState = &blendState;
    info.pDepthStencilState = &NoDepthStencil;
    info.pMultisampleState = &msaaInfo;
    info.pInputAssemblyState = &TriangleListNoRestart;
    info.pVertexInputState = &VertexInputInfo;
    info.pRasterizationState = &rast;

    info.stageCount = lengthof(stages);
    info.pStages = stages;

    VERIFY_VK(vkCreateGraphicsPipelines(device, VkPipelineCache(), 1, &info, ALLOC_CBS, pPipline));
}


bool TestExtRasterMultisample(VkDevice device, VkQueue queue, uint32_t graphicsFamilyIndex, const VkPhysicalDeviceMemoryProperties& memProps)
{
    const VkExtent3D ImageSize = { 64, 64, 1 };
    const VkFormat Format = VK_FORMAT_R16_UINT;

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

    BufferAndMemory stage;
    const uint32_t PackedImageByteSize = ImageSize.width * ImageSize.height * sizeof(uint16_t);
    CreateBufferAndMemory(device, memProps,
                          VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                          PackedImageByteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stage);

    BufferAndMemory attribs;
    {

        CreateBufferAndMemory(device, memProps,
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              4096, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &attribs);
        void *pAttribData;
        VERIFY_VK(vkMapMemory(device, attribs.memory, 0, VK_WHOLE_SIZE, 0, &pAttribData));

        static const vec2f Positions[6] = {
            {  0,    1 },
            {  1,    0 },
            { -0.5, -1 },

            { -0.75,  1 },
            {  1, -1 },
            { -1, -0.75 },
        };
        memcpy(pAttribData, Positions, sizeof Positions);
    }


    ImageAndMemory resource;
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
        CreateImageAndMemory(device, memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageInfo, &resource);
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

    VkImageView view;
    {
        VkImageViewCreateInfo viewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0, // VkImageViewCreateFlags
            resource.image,
            VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            Format,
            VkComponentMapping(), // identity
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // single mip, single layer
        };
        VERIFY_VK(vkCreateImageView(device, &viewCreateInfo, ALLOC_CBS, &view));
    }

    VkFramebuffer framebuffer;
    {
        VkFramebufferCreateInfo framebufferInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0,
            renderpass,
            1, &view,
            ImageSize.width, ImageSize.height, 1
        };
        VERIFY_VK(vkCreateFramebuffer(device, &framebufferInfo, ALLOC_CBS, &framebuffer));
    }


    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    {
        // no descriptors or push constants:
        VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        VERIFY_VK(vkCreatePipelineLayout(device, &info, ALLOC_CBS, &pipelineLayout));
    }

    VkPipeline pipeline = VK_NULL_HANDLE;
    {
        VkShaderModule vs = CreateShaderModule(device, VsSpirv);
        VkShaderModule fs = CreateShaderModule(device, FsSpirv);
        CreateExtRasterMultisamplePipeline(device, vs, fs, renderpass, pipelineLayout, VK_SAMPLE_COUNT_2_BIT, &pipeline);
        vkDestroyShaderModule(device, vs, ALLOC_CBS);
        vkDestroyShaderModule(device, fs, ALLOC_CBS);
    }

    {
        const VkCommandBufferBeginInfo cmdBufbeginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
        };
        VERIFY_VK(vkBeginCommandBuffer(cmdbuf, &cmdBufbeginInfo));

        VkImageMemoryBarrier imageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
            0, // srcAccessFlags
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resource.image,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0x0,
                             0, nullptr, 0, nullptr, 1, &imageBarrier);
        {
            const VkRect2D RenderArea = {
                { 0, 0 }, { ImageSize.width, ImageSize.height}
            };
            const VkRenderPassBeginInfo rpBeginInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr,
                renderpass, framebuffer, RenderArea
            };
            vkCmdBeginRenderPass(cmdbuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            {
                VkClearAttachment attClear;
                attClear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                attClear.colorAttachment = 0;
                attClear.clearValue = { };
                VkClearRect clearRect = { RenderArea, 0, 1 }; // layer range
                vkCmdClearAttachments(cmdbuf, 1, &attClear, 1, &clearRect);

                vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                VkViewport vp = { };
                vp.x = 0;
                vp.y = 0 + int32_t(RenderArea.extent.height);
                vp.width = RenderArea.extent.width;
                vp.height = -int32_t(RenderArea.extent.height);
                vp.minDepth = 0;
                vp.maxDepth = 1;
                vkCmdSetViewport(cmdbuf, 0, 1, &vp);
                vkCmdSetScissor(cmdbuf, 0, 1, &RenderArea);
                VkDeviceSize offsets[1] = { };
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &attribs.buffer, offsets);
                vkCmdDraw(cmdbuf, 6, 1, 0, 0);
            }
            vkCmdEndRenderPass(cmdbuf);
        }
        VkMemoryBarrier memBarrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        };
        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                             1, &memBarrier, 0, nullptr, 0 , nullptr);
        VkBufferImageCopy bufImgCopy = { };
        bufImgCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; // mip, layer{begin, count}
        bufImgCopy.imageExtent = ImageSize;
        bufImgCopy.bufferRowLength = ImageSize.width;
        bufImgCopy.bufferImageHeight = ImageSize.height;
        vkCmdCopyImageToBuffer(cmdbuf, resource.image, VK_IMAGE_LAYOUT_GENERAL, stage.buffer, 1, &bufImgCopy);
        memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0x0,
                             1, &memBarrier, 0, nullptr, 0 , nullptr);

        VERIFY_VK(vkEndCommandBuffer(cmdbuf));
    }

    {
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        VERIFY_VK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        VERIFY_VK(vkQueueWaitIdle(queue));
    }

    void *pMap = nullptr;
    {
        const VkDeviceSize offset = 0, size = PackedImageByteSize;
        VERIFY_VK(vkMapMemory(device, stage.memory, offset, size, 0, &pMap));
        VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, stage.memory, 0x0, VK_WHOLE_SIZE };
        vkInvalidateMappedMemoryRanges(device, 1, &range);
    }

    bool bTestPassed = false;
    {
        static const PackedR8G8B8 ColorFromCoverageMask2[1 << 2] = {
            { 0x00, 0x00, 0x00 },
            { 0xff, 0x00, 0x00 },
            { 0x00, 0xff, 0x00 },
            { 0x00, 0x00, 0xff },
        };

        const uint32_t nPixelsTotal = ImageSize.width * ImageSize.height;
            PackedR8G8B8 *pRgb = (PackedR8G8B8 *)malloc(nPixelsTotal * sizeof(PackedR8G8B8));

        const bool bGenRef = false;
        if (bGenRef) {
            bool bGotBadVaue = false;

            for (uint32_t i = 0; i < nPixelsTotal; ++i) {
                uint16_t genVal = reinterpret_cast<const uint16_t *>(pMap)[i];
                PackedR8G8B8 color;
                if (genVal >= 4) {
                    bGotBadVaue = true;
                    color = { 0xff, 0xff, 0xff };
                } else {
                    color = ColorFromCoverageMask2[genVal];
                }
                pRgb[i] = color;
            }
            if (bGotBadVaue) {
                puts("Got value outside of raster sample pattern!");
            }
            bTestPassed = !bGotBadVaue;
            stbi_write_png("reference_2x.png", ImageSize.width, ImageSize.height, 3, pRgb, ImageSize.width * sizeof(PackedR8G8B8));
        } else {
            int w, h, ncomps;
            void *const pRefData = stbi_load("reference_2x.png", &w, &h, &ncomps, 3);
            if (pRefData && uint32_t(w) == ImageSize.width && uint32_t(h) == ImageSize.height && ncomps == 3) {
                bool bGotBadVaue = false;
                uint32_t nPixelsMatching = 0;
                for (uint32_t i = 0; i < nPixelsTotal; ++i) {
                    uint16_t genVal = reinterpret_cast<const uint16_t *>(pMap)[i];
                    PackedR8G8B8 const refColor = reinterpret_cast<const PackedR8G8B8 *>(pRefData)[i];
                    PackedR8G8B8 color;
                    if (genVal >= 4) {
                        bGotBadVaue = true;
                        color = { 0xff, 0xff, 0xff };
                    } else {
                        color = ColorFromCoverageMask2[genVal];
                    }
                    pRgb[i] = color;
                    nPixelsMatching += (refColor == color);
                }
                if (bGotBadVaue) {
                    puts("Got value outside of raster sample pattern!");
                }
                bTestPassed = !bGotBadVaue && (nPixelsMatching == nPixelsTotal);
                printf("Num pixels matching ref image: %d\n", nPixelsMatching);
                stbi_write_png("generated_2x.png", ImageSize.width, ImageSize.height, 3, pRgb, ImageSize.width * sizeof(PackedR8G8B8));
            } else {
                puts("Failed to load reference_2x.png from cwd.");
            }
        }
        free(pRgb);
    }

    vkDestroyPipeline(device, pipeline, ALLOC_CBS);
    vkDestroyPipelineLayout(device, pipelineLayout, ALLOC_CBS);

    vkFreeCommandBuffers(device, cmdpool, 1, &cmdbuf);
    vkDestroyCommandPool(device, cmdpool, ALLOC_CBS);
    vkDestroyImageView(device, view, ALLOC_CBS);
    vkDestroyFramebuffer(device, framebuffer, ALLOC_CBS);
    vkDestroyRenderPass(device, renderpass, ALLOC_CBS);
    DestroyImageAndFreeMemory(device, resource);
    DestroyBufferAndFreeMemory(device, stage);
    DestroyBufferAndFreeMemory(device, attribs);

    return bTestPassed;
}

