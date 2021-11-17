Work in progress.

Notes:

https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#TIR (Target Independent Rasterization)
https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_raster_multisample.txt
https://www.khronos.org/registry/OpenGL/extensions/NV/NV_framebuffer_mixed_samples.txt
https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_NV_coverage_reduction_mode.html


--------------------------------------------------------------------------------
## VK_EXT_raster_multisample - device extension

Contributors:

- Jonathan Weinstein, VMware
- More welcome, I'll probably need help :)

### Description:

This extension allows a specific case of rendering with a rasterization sample
count >= 2 (up to 16 is often useful) with single-sample color attachment(s)
under some conditions. This enables more pixels along the edges of primitives to
be shaded, and the shader can use the builtin input SampleMask as a way to
determine how covered they are. A use case for this is anti-aliased path
rendering. Similar functionality also exists in other APIs, such as
GL_EXT_raster_multisample form OpenGL and the primary mode of
"Target Independent Rasterization" that is required in Direct3D feature
levels 11_1 through 12_2 (the highest as of this writing).

This extension could be considered a subset of VK_NV_framebuffer_mixed_samples
and requires most depth/stencil state to be disabled.

### New Structures:

```C
struct VkPhysicalDeviceRasterMultisamplePropertiesEXT {
   VkStructureType sType;
   void *pNext;
   VkSampleFlags forcedSamples; // must not contain VK_SAMPLE_COUNT_1_BIT
};
```

Extending VkPipelineMultisampleStateCreateFlags:
   `VK_PIPELINE_MULTISAMPLE_STATE_CREATE_RASTER_MULTISAMPLE_BIT_EXT = 0x00000001`


### Valid Usage:

The application must ensure that when
`VkPipelineMultisampleStateCreateInfo::flags` contains
`VK_PIPELINE_MULTISAMPLE_STATE_CREATE_RASTER_MULTISAMPLE_BIT_EXT`:

- rasterizationSamples must be in `VkPhysicalDeviceRasterMultisamplePropertiesEXT::forcedSamples`, which should only contain values >= 2.
- The subpass must have at least one color attachment and all must have a sample count of 1.
- The subpass must not have a depth or stencil attachment.
- If a fragment shader is present, it must not run at sample-rate and must not output FragDepth or FragStencilRefEXT.
- depthTestEnable, stencilTestEnable, depthBoundsTestEnable must all be VK_FALSE.

### Behavior:

When a pipeline's `VkPipelineMultisampleStateCreateInfo::flags` contains
`VK_PIPELINE_MULTISAMPLE_STATE_CREATE_RASTER_MULTISAMPLE_BIT_EXT`,
rasterization, the builitin input SampleMask, and centroid interpolation
are all based on the pipline's
VkPipelineMultisampleStateCreateInfo::rasterizationSamples pattern.
Like regular MSAA, a single fragment shader is invoked for a pixel
when a primitive covers any sample in the rasterizationSamples pattern.
Assuming a discard, demote-to-helper or similar does not happen, the single
physical color sample is written (optionally with blending or logic ops) if
the shader does not declare output SampleMask, or the final value
or output SampleMask intersects input SampleMask
(XXX: I think that is the usual rule?).

Any `VkPipelineCoverageReductionStateCreateInfoNV` structure is ignored, or the
default coverage-reduction mode based on other extensions does not apply.

TODO: talk about queries here?

### Issues:

1). Why does `VK_PIPELINE_MULTISAMPLE_STATE_CREATE_RASTER_MULTISAMPLE_BIT_EXT`
exist?

Otherwise there would be an ambiguity when e.g VK_AMD_mixed_attachment_samples
is enabled.

2). Can't the feature from this extension be exposed through the existing
VK_NV_coverage_reduction_mode by adding a new VkCoverageReductionModeNV
value that has all the restrictions and behavior of this feature?

Perhaps; this extension can be changed to use that if other's prefer so.
Querying support via vkGetPhysicalDeviceProperties2 and adding a flag to the
existing `VkPipelineMultisampleStateCreateInfo::flags` seemed simpler than using
vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV and chaining
a VkFramebufferMixedSamplesCombinationNV to VkPipelineMultisampleStateCreateInfo.
I don't think any implementors that support some form of mixed samples are
currently looking at a VkFramebufferMixedSamplesCombinationNV in the pipeline
state since I think they only support one of the two existing modes.
