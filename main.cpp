/*
g++ -DVK_NO_PROTOTYPES  *.cpp -std=c++11 -Wall -Wshadow -ldl -o vktest.out

Run via:

`./vktest [gpu_index]`
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
bool TestUavLoadOob(VkDevice device, VkQueue queue, uint32_t graphicsFamilyIndex, const VkPhysicalDeviceMemoryProperties& memProps);

bool TestClipDistanceIo(VkDevice device, VkQueue queue, uint32_t graphicsFamilyIndex, const VkPhysicalDeviceMemoryProperties& memProps);

bool TestXfbPingPong(const VulkanObjetcs& vk);

#include "thirdparty/renderdoc_app.h"
extern RENDERDOC_API_1_1_2 *rdoc_api;

RENDERDOC_API_1_1_2 *rdoc_api = NULL;
#include <dlfcn.h>

int main(int argc, char **argv)
{
    int const tailc = argc - 1;
    char **const tailv = argv + 1;

    int gpuIndex = -1;

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

#ifdef __linux__
    if (1) {
        const char *renderdocLibPath = "librenderdoc.so";
        if (void *mod = dlopen(renderdocLibPath, RTLD_NOW | RTLD_NOLOAD)) {
            printf("Detected \"%s\" was loaded.\n", renderdocLibPath);
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
            int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
            if (ret != 1) {
                printf("error: RENDERDOC_GetAPI for version=1.1.2 returned %d\n", ret);
                rdoc_api = NULL;
            }
        } else {
            const char *err = dlerror();
            fprintf(stderr, "Failed to dlopen(\"%s\", RTLD_NOW | RTLD_NOLOAD), dlerror=\"%s\"; try launching the process from renderdoc.\n",
                    renderdocLibPath, err ? err : "<NULL>");
        }
    }
#endif

    VulkanObjetcs vk;
    VkResult const initResult = SimpleInitVulkan(&vk, ~0u, gpuIndex, GpuVendorID::Intel);
    if (initResult == VK_SUCCESS) {
        fflush(stderr);
        bool passed;
        if (0) {
            puts("Running test VK_EXT_raster_multisample..."); fflush(stdout);
            passed = TestExtRasterMultisample(vk.device, vk.universalQueue, vk.universalFamilyIndex, vk.memProps);
            puts(passed ? "Test PASSED." : "\nTest FAILED."); fflush(stdout);
        }

        if (0) { // XXX
            if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
            //TestClipDistanceIo(vk.device, vk.universalQueue, vk.universalFamilyIndex, vk.memProps);
            TestXfbPingPong(vk);
            if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
        }

        if (1) { // run this by default for now, might want to update that page
            if (vk.robustness2Features.robustImageAccess2) {
                puts("Running test ld_typed_2darray_oob..."); fflush(stdout);
                passed = TestUavLoadOob(vk.device, vk.universalQueue, vk.universalFamilyIndex, vk.memProps);
                puts(passed ? "Test PASSED." : "\nTest FAILED."); fflush(stdout);
            } else {
                puts("robustImageAccess2 not supported, skipping test ld_typed_2darray_oob.");
            }
        }
    } else {
        printf("Failed to initialize Vulkan, VkResult = %d\n", initResult);
    }

    SimpleDestroyVulkan(&vk);
    return 0;
}
