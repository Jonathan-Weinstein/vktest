#include "vk_simple_init.h"
#include "volk/volk.h"
#include "vk_util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "stb/stb_image_write.h"

extern bool g_bSaveFailingImages;

#ifdef _WIN32
#include <direct.h>
static char *GetCwd(char *buf, int n) { return _getcwd(buf, n); }
#else
#include <unistd.h>
static char *GetCwd(char *buf, int n) { return getcwd(buf, n); }
#endif


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

template<unsigned WordCount> VkShaderModule
CreateShaderModule(VkDevice device, const uint32_t(&a)[WordCount])
{
    return CreateShaderModule(device, WordCount * sizeof(uint32_t), a);
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

#define USE_STATIC_SHADERS
#ifdef USE_STATIC_SHADERS
/* NOTE: following made from custom spirv assembly:
{
    // theta = pi/16 radians
    out.xy = vec2(cos_theta, sin_theta) * in.x + vec2(-sin_theta, cos_theta);
    out.z = in.z + 1/32.0f;
    out.w = in.w;
}
*/
/*
; SPIR-V
; Version: 1.3
; Generator: Khronos SPIR-V Tools Assembler; 0
; Bound: 31
; Schema: 0
               OpCapability Shader
               OpCapability TransformFeedback
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %1 "main" %2 %gl_Position
               OpExecutionMode %1 Xfb
               OpDecorate %2 Location 0
               OpDecorate %gl_Position BuiltIn Position
               OpDecorate %gl_Position XfbBuffer 0
               OpDecorate %gl_Position XfbStride 16
               OpDecorate %gl_Position Offset 0
       %void = OpTypeVoid
          %5 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
%_ptr_Input_v4float = OpTypePointer Input %v4float
%_ptr_Output_v4float = OpTypePointer Output %v4float
          %2 = OpVariable %_ptr_Input_v4float Input
%gl_Position = OpVariable %_ptr_Output_v4float Output
%float_0_03125 = OpConstant %float 0.03125
%float_0_195090324 = OpConstant %float 0.195090324
%float_0_980785251 = OpConstant %float 0.980785251
%float_n0_195090324 = OpConstant %float -0.195090324
         %16 = OpConstantComposite %v2float %float_0_980785251 %float_0_195090324
         %17 = OpConstantComposite %v2float %float_n0_195090324 %float_0_980785251
          %1 = OpFunction %void None %5
         %18 = OpLabel
         %19 = OpLoad %v4float %2
         %20 = OpCompositeExtract %float %19 0
         %21 = OpVectorTimesScalar %v2float %16 %20
         %22 = OpCompositeExtract %float %19 1
         %23 = OpVectorTimesScalar %v2float %17 %22
         %24 = OpFAdd %v2float %21 %23
         %25 = OpCompositeExtract %float %19 2
         %26 = OpCompositeExtract %float %24 0
         %27 = OpCompositeExtract %float %24 1
         %28 = OpFAdd %float %25 %float_0_03125
         %29 = OpCompositeExtract %float %19 3
         %30 = OpCompositeConstruct %v4float %26 %27 %28 %29
               OpStore %gl_Position %30
               OpReturn
               OpFunctionEnd
*/
static const uint32_t VsXfbSpirv[] =
{
0x7230203,0x10300,0x70000,31,0,0x20011,1,0x20011,53,0x3000E,0,1,0x7000F,0,1,0x6E69616D,0,2,3,0x30010,1,11,
0x40047,2,30,0,0x40047,3,11,0,0x40047,3,36,0,0x40047,3,37,16,0x40047,3,35,0,0x20013,4,0x30021,5,4,0x30016,6,32,
0x40017,7,6,2,0x40017,8,6,4,0x40015,9,32,0,0x40020,10,1,8,0x40020,11,3,8,0x4003B,10,2,1,0x4003B,11,3,3,0x4002B,
6,12,0x3D000000,0x4002B,6,13,0x3E47C5C2,0x4002B,6,14,0x3F7B14BE,0x4002B,6,15,0xBE47C5C2,0x5002C,7,16,14,13,
0x5002C,7,17,15,14,0x50036,4,1,0,5,0x200F8,18,0x4003D,8,19,2,0x50051,6,20,19,0,0x5008E,7,21,16,20,0x50051,6,22,
19,1,0x5008E,7,23,17,22,0x50081,7,24,21,23,0x50051,6,25,19,2,0x50051,6,26,24,0,0x50051,6,27,24,1,0x50081,6,28,
25,12,0x50051,6,29,19,3,0x70050,8,30,26,27,28,29,0x3003E,3,30,0x100FD,0x10038,
};

/*
// dxc -spirv -fspv-target-env=vulkan1.1 -T vs_6_0 vs_plain.hlsl
void main(in float4 input0 : input0,
          out float4 pos : SV_Position)
{
    pos = input0;
}
; SPIR-V
; Version: 1.3
; Generator: Google spiregg; 0
; Bound: 12
; Schema: 0
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %in_var_input0 %gl_Position
               OpSource HLSL 600
               OpName %in_var_input0 "in.var.input0"
               OpName %main "main"
               OpDecorate %gl_Position BuiltIn Position
               OpDecorate %in_var_input0 Location 0
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Input_v4float = OpTypePointer Input %v4float
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
          %9 = OpTypeFunction %void
%in_var_input0 = OpVariable %_ptr_Input_v4float Input
%gl_Position = OpVariable %_ptr_Output_v4float Output
       %main = OpFunction %void None %9
         %10 = OpLabel
         %11 = OpLoad %v4float %in_var_input0
               OpStore %gl_Position %11
               OpReturn
               OpFunctionEnd
*/
static const uint32_t VsPlainSpirv[] =
{
0x7230203,0x10300,0xE0000,12,0,0x20011,1,0x3000E,0,1,0x7000F,0,1,0x6E69616D,0,2,3,0x30003,5,0x258,0x60005,2,
0x762E6E69,0x692E7261,0x7475706E,48,0x40005,1,0x6E69616D,0,0x40047,3,11,0,0x40047,2,30,0,0x30016,4,32,0x40017,5,
4,4,0x40020,6,1,5,0x40020,7,3,5,0x20013,8,0x30021,9,8,0x4003B,6,2,1,0x4003B,7,3,3,0x50036,8,1,0,9,0x200F8,10,
0x4003D,5,11,2,0x3003E,3,11,0x100FD,0x10038
};

/*
// dxc -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 fs.hlsl
void main(out float4 rtv0 : SV_Target)
{
    rtv0 = float4(1.0f, 0.5f, 0.25f, 1.0f);
}
; SPIR-V
; Version: 1.3
; Generator: Google spiregg; 0
; Bound: 13
; Schema: 0
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %out_var_SV_Target
               OpExecutionMode %main OriginUpperLeft
               OpSource HLSL 600
               OpName %out_var_SV_Target "out.var.SV_Target"
               OpName %main "main"
               OpDecorate %out_var_SV_Target Location 0
      %float = OpTypeFloat 32
    %float_1 = OpConstant %float 1
  %float_0_5 = OpConstant %float 0.5
 %float_0_25 = OpConstant %float 0.25
    %v4float = OpTypeVector %float 4
          %8 = OpConstantComposite %v4float %float_1 %float_0_5 %float_0_25 %float_1
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
         %11 = OpTypeFunction %void
%out_var_SV_Target = OpVariable %_ptr_Output_v4float Output
       %main = OpFunction %void None %11
         %12 = OpLabel
               OpStore %out_var_SV_Target %8
               OpReturn
               OpFunctionEnd
*/
static const uint32_t FsSpirv[] =
{
0x7230203,0x10300,0xE0000,13,0,0x20011,1,0x3000E,0,1,0x6000F,4,1,0x6E69616D,0,2,0x30010,1,7,0x30003,5,0x258,
0x70005,2,0x2E74756F,0x2E726176,0x545F5653,0x65677261,0x74,0x40005,1,0x6E69616D,0,0x40047,2,30,0,0x30016,3,32,
0x4002B,3,4,0x3F800000,0x4002B,3,5,0x3F000000,0x4002B,3,6,0x3E800000,0x40017,7,3,4,0x7002C,7,8,4,5,6,4,0x40020,
9,3,7,0x20013,10,0x30021,11,10,0x4003B,9,2,3,0x50036,10,1,0,11,0x200F8,12,0x3003E,2,8,0x100FD,0x10038
};
#endif


struct Vertex {
   float x, y, z, w;
};

static const Vertex Verts[] = {
   { -1.0f, -0.5f, 0,       1.0f },
   { -0.5f,  0.5f, 1/32.0f, 1.0f },
   {  0.0f, -0.5f, 2/32.0f, 1.0f },
   {  0.5f,  0.5f, 3/32.0f, 1.0f },
   {  1.0f, -0.5f, 4/32.0f, 1.0f }
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
    const VkExtent3D ImageSize = { 256, 256, 1 };
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
    const uint32_t StageByteCapacity = PackedImageByteSize + sizeof Verts;
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
        memcpy(pMapMem[0], Verts, sizeof Verts);
        memset(pMapMem[1], 0, sizeof Verts);
        for (const auto& r : buffers) { vkUnmapMemory(device, r.memory); }
    }

    /* The counter doesn't really do anything in this test, but in most engines one is always provided in CmdEndXfb(),
    so we do it too in case there are any sideffects to that. It is not loaded from here in CmdBeginXfb(): */
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

#ifdef USE_STATIC_SHADERS
    VkShaderModule vs_xfb = VK_NULL_HANDLE;
    {
        uint32_t tmp[lengthof(VsXfbSpirv)];
        memcpy(tmp, VsXfbSpirv, sizeof tmp);
        if (0) { // this doesn't seem to fix the issue:
            /* Can do this because rasterizerDiscard is enabled: */
            puts("Replacing 'OpDecorate %output BuiltIn Position' with 'OpDecorate %output Location 0'.");
            tmp[0x00000068 / 4 + 2] = 30; // SpvDecorationLocation
            tmp[0x00000068 / 4 + 3] = 0;
        } else {
            // leave as: OpDecorate %gl_Position BuiltIn Position ; 0x00000068
            puts("Using variant with Postion as XFB output.");
        }
        vs_xfb = CreateShaderModule(device, tmp);
    }
    VkShaderModule vs_plain = CreateShaderModule(device, VsPlainSpirv);
    VkShaderModule fs = CreateShaderModule(device, FsSpirv);
    (void)CreateShaderModuleFromFile;
#else
    puts("Using shaders from filesystem.");
    VkShaderModule vs_xfb = CreateShaderModuleFromFile(device, "shaders/xfb/custom_vs_xfb.spv");
    VkShaderModule vs_plain = CreateShaderModuleFromFile(device, "shaders/xfb/vs_plain.spv");
    VkShaderModule fs = CreateShaderModuleFromFile(device, "shaders/xfb/fs.spv");
#endif

    VkPipeline pso_xfb = VK_NULL_HANDLE;
    VkPipeline pso_rast = VK_NULL_HANDLE;
    CreateGraphicsPipeline(device, vs_xfb, VK_NULL_HANDLE, emptyRenderpass, pipelineLayout, &pso_xfb);
    CreateGraphicsPipeline(device, vs_plain, fs, renderpass, pipelineLayout, &pso_rast);

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

    const int NumIters = 16;
    /* Changing these don't seem to make a difference: */
    const bool bSingleBarrierCall = true;
    const bool bMinimalSrcStageMask = false;
    printf("bSingleBarrierCall=%d, bMinimalSrcStageMask=%d, numIters=%d\n",
            int(bSingleBarrierCall), int(bMinimalSrcStageMask), NumIters);

    VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, 0, 0 };
    auto CmdGlobalBarrier = [cmdbuf, &barrier](VkPipelineStageFlags srcStages, VkPipelineStageFlags dstStages) {
        vkCmdPipelineBarrier(cmdbuf, srcStages, dstStages, 0x0, 1, &barrier, 0, nullptr, 0 , nullptr);
    };
    vkuCmdLabel(vkCmdBeginDebugUtilsLabelEXT, cmdbuf, "XFB loop", 0xff0000ffu);
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pso_xfb);
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &buffers[1].buffer, ZeroOffsets);

    for (int i = 0; i < NumIters; ++i) {
        if (i != 0) {
            if (bSingleBarrierCall) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT |
                                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
                /* I think VK_PIPELINE_STAGE_VERTEX_INPUT_BIT in src stages should not be necessary in conjunction with XFB
                in src stages since XFB is a later stage, but it's here for good measure if configured: */
                CmdGlobalBarrier(VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT | (bMinimalSrcStageMask ? 0 : VK_PIPELINE_STAGE_VERTEX_INPUT_BIT),
                                 VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
            } else {
                /* XfbWrite -> VertexRead: */
                barrier.srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
                barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
                CmdGlobalBarrier(VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
                /* VertexRead -> XfbWrite. Execution only, no access mask (note a WAR with image layout transation
                * would need nonzero dstAccessMask since layout transition is considered a write operation). */
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = 0;
                CmdGlobalBarrier(VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT);
                /* EndXfbCounterWrite -> EndXfbCounterWrite (counter never read at BeginXfb in this sample): */
                barrier.srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
                CmdGlobalBarrier(VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                                 VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT);
            }
        }
        vkCmdBeginRenderPass(cmdbuf, &EmptyFramebufferBeginRenderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &buffers[i & 1].buffer, ZeroOffsets);
        vkCmdBindTransformFeedbackBuffersEXT(cmdbuf, 0, 1, &buffers[(i & 1) ^ 1].buffer, ZeroOffsets, WholeSizes);
        vkCmdBeginTransformFeedbackEXT(cmdbuf, 0, 1, nullptr, nullptr); // don't load from counters to add onto offset
        vkCmdDraw(cmdbuf, 5, 1, 0, 0);
        vkCmdEndTransformFeedbackEXT(cmdbuf, 0, 1, &xfbCounter.buffer, ZeroOffsets);
        vkCmdEndRenderPass(cmdbuf);
    }
    vkCmdEndDebugUtilsLabelEXT(cmdbuf);
    VkBuffer const lastXfbTargetBuffer = buffers[NumIters & 1].buffer;
    barrier.srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0x0,
                         1, &barrier, 0, nullptr, 0 , nullptr);
    // done later: vkCmdBindVertexBuffers(cmdbuf, 0, 1, &lastXfbTargetBuffer, ZeroOffsets);

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
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &lastXfbTargetBuffer, ZeroOffsets);
        vkCmdDraw(cmdbuf, 5, 1, 0, 0);
    }
    vkCmdEndRenderPass(cmdbuf);

    imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                         0, nullptr, 0, nullptr, 1, &imageBarrier);

    {
        VkBufferImageCopy bufImgCopy = { };
        bufImgCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; // mip, layer{begin, count}
        bufImgCopy.imageExtent = ImageSize;
        bufImgCopy.bufferOffset = 0;
        bufImgCopy.bufferRowLength = ImageSize.width;
        bufImgCopy.bufferImageHeight = ImageSize.height;
        vkCmdCopyImageToBuffer(cmdbuf, outputImag.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               stage.buffer, 1, &bufImgCopy);
    }

    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0x0,
                         1, &barrier, 0, nullptr, 0 , nullptr);
    {
        VkBufferCopy bufCopy = { };
        bufCopy.size = sizeof Verts;
        bufCopy.dstOffset = PackedImageByteSize;
        bufCopy.srcOffset = 0;
        vkCmdCopyBuffer(cmdbuf, lastXfbTargetBuffer, stage.buffer, 1, &bufCopy);
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
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        VERIFY_VK(vkQueueSubmit(vk.universalQueue, 1, &submitInfo, VK_NULL_HANDLE));
        VERIFY_VK(vkQueueWaitIdle(vk.universalQueue));
        const VkDeviceSize offset = 0, size = StageByteCapacity;
        VERIFY_VK(vkMapMemory(device, stage.memory, offset, size, 0, &pMap));
        const VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, stage.memory, offset, size };
        vkInvalidateMappedMemoryRanges(device, 1, &range);
    }

    bool failed = false;
    {
        const Vertex *const data = (const Vertex *)((const char *)pMap + PackedImageByteSize);
        puts("");
        for (unsigned i = 0; i < lengthof(Verts); ++i) {
            Vertex const v = data[i];
            printf("data[%d] = {%f %f %f %f}\n", i, v.x, v.y, v.z, v.w);
            if (v.w != 1.0f) {
                failed = true;
                printf("BAD OUTPUT: data[%d].w should be 1.0f, is %f\n", i, v.w);
            }

            const float expect = Verts[i].z + NumIters/32.0f;
            if (v.z != expect) {
                failed = true;
                printf("BAD OUTPUT: data[%d].z should be %f (%f + %d/32.0f), is %f\n", i,
                    expect, Verts[i].z, NumIters,
                    v.z);
            }
        }
    }

    if (failed) {
        puts("\n *** Test FAILED ***");
        if (g_bSaveFailingImages) {
            const char *relName = "xfb_output.png";
            char cwdbuf[4096];
            printf("Writing (file)=(%s) from (cwd)=(%s)\n", relName, GetCwd(cwdbuf, sizeof cwdbuf));
            stbi_write_png(relName, ImageSize.width, ImageSize.height, 4,
                        (const unsigned char *)pMap + 0*PackedImageByteSize, ImageSize.width*sizeof(uint32_t));
        } else {
            puts("Not saving output image, pass --save-failing-images if desired.");
        }
        puts("");
    }

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
    printf("Leaving function %s\n", __FUNCTION__);
    fflush(stdout);
    return !failed;
}

/*
Self note(JW): the same VS was used in the rasterize pass of
streamout_recursive_xformt. That's where the extra rotation came from
and loop count was 1 less.
*/
