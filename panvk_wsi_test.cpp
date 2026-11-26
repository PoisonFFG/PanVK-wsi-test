#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

#define TAG "PanVK_WSI_TEST"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static std::string resultName(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        default: return "VkResult(" + std::to_string((int)r) + ")";
    }
}

static bool hasInstanceExtension(const char* name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> props(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data()) != VK_SUCCESS) return false;
    for (const auto& p : props) if (std::strcmp(p.extensionName, name) == 0) return true;
    return false;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_panvkwsi_MainActivity_runTest(JNIEnv* env, jobject, jobject javaSurface) {
    std::ostringstream out;
    ANativeWindow* window = ANativeWindow_fromSurface(env, javaSurface);
    if (!window) return env->NewStringUTF("FAIL: ANativeWindow_fromSurface returned null.");

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
    VkSemaphore ready = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;

    auto fail = [&](const std::string& where, VkResult r) {
        out << "\\nFAIL at " << where << ": " << resultName(r) << " (" << (int)r << ")";
        LOGE("FAIL at %s: %s (%d)", where.c_str(), resultName(r).c_str(), (int)r);
    };

    out << "PanVK Android WSI diagnostic\\n";
    out << "Native window: " << ANativeWindow_getWidth(window) << " x "
        << ANativeWindow_getHeight(window) << "\\n";

    const char* instanceExts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };
    if (!hasInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME) ||
        !hasInstanceExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
        out << "\\nFAIL: Vulkan loader does not expose VK_KHR_surface and/or VK_KHR_android_surface.";
        goto cleanup;
    }

    {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "PanVK WSI Test";
        app.applicationVersion = 1;
        app.pEngineName = "Standalone Vulkan diagnostic";
        app.engineVersion = 1;
        app.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo = &app;
        ci.enabledExtensionCount = 2;
        ci.ppEnabledExtensionNames = instanceExts;
        VkResult r = vkCreateInstance(&ci, nullptr, &instance);
        if (r != VK_SUCCESS) { fail("vkCreateInstance", r); goto cleanup; }
        out << "vkCreateInstance: PASS\\n";
    }

    {
        auto createAndroidSurface = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
            vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR"));
        if (!createAndroidSurface) {
            out << "\\nFAIL: vkCreateAndroidSurfaceKHR function pointer is null.";
            goto cleanup;
        }
        VkAndroidSurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
        sci.window = window;
        VkResult r = createAndroidSurface(instance, &sci, nullptr, &surface);
        if (r != VK_SUCCESS) { fail("vkCreateAndroidSurfaceKHR", r); goto cleanup; }
        out << "vkCreateAndroidSurfaceKHR: PASS\\n";
    }

    {
        uint32_t count = 0;
        VkResult r = vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (r != VK_SUCCESS || count == 0) {
            if (r == VK_SUCCESS) r = VK_ERROR_INITIALIZATION_FAILED;
            fail("vkEnumeratePhysicalDevices", r); goto cleanup;
        }
        std::vector<VkPhysicalDevice> gpus(count);
        r = vkEnumeratePhysicalDevices(instance, &count, gpus.data());
        if (r != VK_SUCCESS) { fail("vkEnumeratePhysicalDevices(data)", r); goto cleanup; }

        VkPhysicalDevice physical = VK_NULL_HANDLE;
        uint32_t queueFamily = UINT32_MAX;
        VkPhysicalDeviceProperties props{};
        for (VkPhysicalDevice candidate : gpus) {
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(candidate, &p);
            uint32_t qcount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qcount, nullptr);
            std::vector<VkQueueFamilyProperties> qprops(qcount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qcount, qprops.data());
            for (uint32_t q = 0; q < qcount; ++q) {
                VkBool32 present = VK_FALSE;
                VkResult pr = vkGetPhysicalDeviceSurfaceSupportKHR(candidate, q, surface, &present);
                if (pr == VK_SUCCESS && present && (qprops[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    physical = candidate;
                    queueFamily = q;
                    props = p;
                    break;
                }
            }
            if (physical != VK_NULL_HANDLE) break;
        }
        if (physical == VK_NULL_HANDLE) {
            out << "\\nFAIL: no queue family supports both GRAPHICS and presentation.";
            goto cleanup;
        }
        out << "GPU: " << props.deviceName << "\\n";
        out << "Vulkan API: " << VK_VERSION_MAJOR(props.apiVersion) << "."
            << VK_VERSION_MINOR(props.apiVersion) << "."
            << VK_VERSION_PATCH(props.apiVersion) << "\\n";
        out << "Graphics + present queue: PASS (family " << queueFamily << ")\\n";

        float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = queueFamily;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;

        const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = deviceExts;
        r = vkCreateDevice(physical, &dci, nullptr, &device);
        if (r != VK_SUCCESS) { fail("vkCreateDevice (VK_KHR_swapchain)", r); goto cleanup; }
        vkGetDeviceQueue(device, queueFamily, 0, &queue);
        out << "vkCreateDevice + VK_KHR_swapchain: PASS\\n";

        uint32_t formatCount = 0;
        r = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, nullptr);
        if (r != VK_SUCCESS || formatCount == 0) {
            if (r == VK_SUCCESS) r = VK_ERROR_FORMAT_NOT_SUPPORTED;
            fail("vkGetPhysicalDeviceSurfaceFormatsKHR", r); goto cleanup;
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        r = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, formats.data());
        if (r != VK_SUCCESS) { fail("vkGetPhysicalDeviceSurfaceFormatsKHR(data)", r); goto cleanup; }

        uint32_t modeCount = 0;
        r = vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &modeCount, nullptr);
        if (r != VK_SUCCESS || modeCount == 0) {
            if (r == VK_SUCCESS) r = VK_ERROR_INITIALIZATION_FAILED;
            fail("vkGetPhysicalDeviceSurfacePresentModesKHR", r); goto cleanup;
        }
        std::vector<VkPresentModeKHR> modes(modeCount);
        r = vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &modeCount, modes.data());
        if (r != VK_SUCCESS) { fail("vkGetPhysicalDeviceSurfacePresentModesKHR(data)", r); goto cleanup; }

        VkSurfaceCapabilitiesKHR caps{};
        r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
        if (r != VK_SUCCESS) { fail("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", r); goto cleanup; }

        VkSurfaceFormatKHR chosen = formats[0];
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_R8G8B8A8_UNORM &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }
        }
        if (chosen.format != VK_FORMAT_R8G8B8A8_UNORM) {
            for (const auto& f : formats) {
                if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
                    f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }
            }
        }

        out << "Surface formats: " << formatCount << "\\n";
        out << "Present modes: " << modeCount << "\\n";
        out << "Chosen format enum: " << (int)chosen.format << "\\n";
        out << "FIFO present mode: "
            << (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR) != modes.end()
                ? "available" : "MISSING") << "\\n";
        if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR) == modes.end()) {
            out << "\\nFAIL: FIFO present mode is not advertised.";
            goto cleanup;
        }

        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if ((caps.supportedUsageFlags & usage) == 0) {
            out << "\\nFAIL: surface does not support COLOR_ATTACHMENT swapchain images. "
                << "supportedUsageFlags=0x" << std::hex << caps.supportedUsageFlags;
            goto cleanup;
        }

        VkExtent2D extent{};
        if (caps.currentExtent.width != UINT32_MAX) {
            extent = caps.currentExtent;
        } else {
            extent.width = std::clamp((uint32_t)ANativeWindow_getWidth(window),
                                      caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp((uint32_t)ANativeWindow_getHeight(window),
                                       caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if ((caps.supportedCompositeAlpha & alpha) == 0) {
            const VkCompositeAlphaFlagBitsKHR candidates[] = {
                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
            };
            for (auto c : candidates) if (caps.supportedCompositeAlpha & c) { alpha = c; break; }
        }

        VkSwapchainCreateInfoKHR swci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swci.surface = surface;
        swci.minImageCount = imageCount;
        swci.imageFormat = chosen.format;
        swci.imageColorSpace = chosen.colorSpace;
        swci.imageExtent = extent;
        swci.imageArrayLayers = 1;
        swci.imageUsage = usage;
        swci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swci.preTransform = caps.currentTransform;
        swci.compositeAlpha = alpha;
        swci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swci.clipped = VK_TRUE;

        r = vkCreateSwapchainKHR(device, &swci, nullptr, &swapchain);
        if (r != VK_SUCCESS) { fail("vkCreateSwapchainKHR", r); goto cleanup; }
        out << "vkCreateSwapchainKHR: PASS\\n";
        out << "Swapchain extent: " << extent.width << " x " << extent.height << "\\n";
        out << "Swapchain images requested: " << imageCount << "\\n";

        VkSemaphoreCreateInfo semci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        r = vkCreateSemaphore(device, &semci, nullptr, &acquired);
        if (r != VK_SUCCESS) { fail("vkCreateSemaphore(acquire)", r); goto cleanup; }
        r = vkCreateSemaphore(device, &semci, nullptr, &ready);
        if (r != VK_SUCCESS) { fail("vkCreateSemaphore(render)", r); goto cleanup; }

        for (int i = 0; i < 3; ++i) {
            uint32_t imageIndex = 0;
            r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired,
                                      VK_NULL_HANDLE, &imageIndex);
            if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
                fail("vkAcquireNextImageKHR", r); goto cleanup;
            }

            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &acquired;
            submit.pWaitDstStageMask = &waitStage;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &ready;
            r = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
            if (r != VK_SUCCESS) { fail("vkQueueSubmit", r); goto cleanup; }

            VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &ready;
            present.swapchainCount = 1;
            present.pSwapchains = &swapchain;
            present.pImageIndices = &imageIndex;
            r = vkQueuePresentKHR(queue, &present);
            if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
                fail("vkQueuePresentKHR", r); goto cleanup;
            }
            vkQueueWaitIdle(queue);
            out << "Frame " << (i + 1) << ": acquire + submit + present PASS\\n";
        }
        out << "\\nRESULT: PASS — Android Vulkan swapchain works independently of DXVK.";
    }

cleanup:
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        if (ready) vkDestroySemaphore(device, ready, nullptr);
        if (acquired) vkDestroySemaphore(device, acquired, nullptr);
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
    }
    if (surface && instance) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
    ANativeWindow_release(window);

    std::string report = out.str();
    LOGI("%s", report.c_str());
    return env->NewStringUTF(report.c_str());
}
