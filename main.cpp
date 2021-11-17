#ifndef VK_NO_PROTOTYPES
#error "Compile with -DVK_NO_PROTOTYPES"
#endif

#include <stdio.h>
#include "vk_simple_init.h"
#include "volk/volk.h"

#include <stdlib.h>

bool TestExtRasterMultisample(VkDevice device, VkQueue queue, uint32_t graphicsFamilyIndex, const VkPhysicalDeviceMemoryProperties& memProps);

static VkSampleCountFlags
GetTirSampleFlagsSupported(VkPhysicalDevice physicalDevice, VkInstance instance)
{
   VkSampleCountFlags sampleFlags = 0;
   PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV fpGetCombos =
      (PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)
         vkGetInstanceProcAddr(instance,
            "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
   if (fpGetCombos) {
      uint32_t nCombos = 0;
      fpGetCombos(physicalDevice, &nCombos, nullptr);
      VkFramebufferMixedSamplesCombinationNV *const pCombos =
         (VkFramebufferMixedSamplesCombinationNV *)calloc(nCombos, sizeof *pCombos);
      /* Validation layer wants this, in addition to all pNext=NULL (calloc): */
      for (uint32_t i = 0; i < nCombos; ++i) {
         pCombos[i].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV;
      }
      fpGetCombos(physicalDevice, &nCombos, pCombos);
      for (uint32_t i = 0; i < nCombos; ++i) {
         if (pCombos[i].coverageReductionMode == VK_COVERAGE_REDUCTION_MODE_MERGE_NV &&
             pCombos[i].depthStencilSamples == 0 &&
             pCombos[i].colorSamples == 1) {
            sampleFlags |= pCombos[i].rasterizationSamples;
         }
      }
      free(pCombos);
   }
   return sampleFlags;
}


int main(int argc, char **argv)
{
    int const tailc = argc - 1;
    char **const tailv = argv + 1;
    printf("tailc=%d, tailv[-1]=[%s]\n", tailc, tailv[-1]);

    VulkanObjetcs vk;
    SimpleInitVulkan(&vk, 0, ~0u);

    if (vk.NV_coverage_reduction_mode) {
        VkSampleCountFlags flags = GetTirSampleFlagsSupported(vk.physicalDevice, vk.instance);
        printf("VK_NV_coverage_reduction_mode TIR SampleFlags={");
        for (uint32_t f = 1; flags; f <<= 1) {
            if (flags & f) {
                printf("%d", f);
                flags &= ~f;
                if (!flags) break;
                putchar(',');
            }
        }
        puts("}");
    }

    TestExtRasterMultisample(vk.device, vk.universalQueue, vk.universalFamilyIndex, vk.memProps);

    SimpleDestroyVulkan(&vk);

    puts("Bye...");

    return 0;
}
