/*
`g++ -DVK_NO_PROTOTYPES  *.cpp -std=c++11 -Wall -Wshadow -ldl -o vktest.out`

Run via:

`./vktest [gpu_index (default = 0)]`
*/


#ifndef VK_NO_PROTOTYPES
#error "Compile with -DVK_NO_PROTOTYPES"
#endif

#include <stdio.h>
#include "vk_simple_init.h"
#include "volk/volk.h"

#include <stdlib.h>
#include <string.h>

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
        #if 0
            printf("combo[%2d]{ mode=%d, raster=%2d, depthStencil=%2d, color=%2d }\n", i, pCombos[i].coverageReductionMode,
            pCombos[i].rasterizationSamples,
            pCombos[i].depthStencilSamples,
            pCombos[i].colorSamples);
        #endif
        }
        free(pCombos);
    }
    return sampleFlags;
}


int main(int argc, char **argv)
{
    int const tailc = argc - 1;
    char **const tailv = argv + 1;

    int gpuIndex = 0;

    if (tailc >= 0) {
        printf("tailc=%d, tailv[-1]=[%s]\n", tailc, tailv[-1]);

        if (tailc >= 2) {
            printf("got %d args, only 0 or 1 valid\n", tailc);
            return 1;
        }

        if (tailc == 1) {
            gpuIndex = atoi(tailv[0]);
        }
    }

    VulkanObjetcs vk;
    VkResult const initResult = SimpleInitVulkan(&vk, gpuIndex, ~0u);
    if (initResult == VK_SUCCESS) {
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

        puts("Running test..."); fflush(stdout);
        bool passed = TestExtRasterMultisample(vk.device, vk.universalQueue, vk.universalFamilyIndex, vk.memProps);
        puts(passed ? "\nTest PASSED." : "\nTest FAILED."); fflush(stdout);
    } else {
        printf("Failed to initialize Vulkan, VkResult = %d\n", initResult);
    }

    SimpleDestroyVulkan(&vk);
    return 0;
}
