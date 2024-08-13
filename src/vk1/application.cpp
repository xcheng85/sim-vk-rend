#include <format>
#include <application.h>
#include <window.h>

#define VK_NO_PROTOTYPES // for volk
#define VOLK_IMPLEMENTATION

// triple-buffer
static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
static constexpr int MAX_DESCRIPTOR_SETS = 1 * MAX_FRAMES_IN_FLIGHT + 1 + 4;
// static constexpr int MAX_DESCRIPTOR_SETS = 1000;
//  Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

void VkApplication::init()
{
    createInstance();
    createSurface();
    selectPhysicalDevice();
    queryPhysicalDeviceCaps();
    selectQueueFamily();
    selectFeatures();
    createLogicDevice();
    cacheCommandQueue();
    createVMA();
    prepareSwapChainCreation();
    createSwapChain();
    createSwapChainImageViews();
    createSwapChainRenderPass();
    createShaderModules();
    createDescriptorSetLayout();
    createDescriptorPool();
    allocateDescriptorSets();
    createUniformBuffers();

    // // application logic
    createGraphicsPipeline();
    // createSwapChainFramebuffers();
    // createCommandPool();
    // createCommandBuffer();
    // createPerFrameSyncObjects();
    // // vao, textures and glb all depends on host-device io
    // // one-time commandBuffer _uploadCmd
    // preHostDeviceIO();
    // loadVao();
    // // must prior to bindResourceToDescriptorSets due to imageView
    // loadTextures();
    // loadGLB();
    // postHostDeviceIO();
    // bindResourceToDescriptorSets();

    _initialized = true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void * /* pUserData */)
{
    // LOGE("MessageID: %s %d \nMessage: %s\n\n", pCallbackData->pMessageIdName,
    //      pCallbackData->messageIdNumber, pCallbackData->pMessage);
    return VK_FALSE;
}

// void VkApplication::teardown()
// {
//     vkDeviceWaitIdle(_logicalDevice);
//     deleteSwapChain();

//     // delete io fence
//     vkDestroyFence(_logicalDevice, _ioFence, nullptr);
//     // free cmdBuffer for io purpose
//     vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &_uploadCmd);

//     // texture
//     vkDestroyImageView(_logicalDevice, _imageView, nullptr);
//     vkDestroyImage(_logicalDevice, _image, nullptr);
//     vkDestroySampler(_logicalDevice, _sampler, nullptr);
//     vmaFreeMemory(_vmaAllocator, _vmaImageAllocation);

//     // glb
//     for (const auto &imageView : _glbImageViews)
//     {
//         vkDestroyImageView(_logicalDevice, imageView, nullptr);
//     }
//     ASSERT(_glbImages.size() == _glbImageAllocation.size(),
//            "_glbImages'size should == _glbImageAllocation's size");
//     for (int i = 0; i < _glbImages.size(); ++i)
//     {
//         vkDestroyImage(_logicalDevice, _glbImages[i], nullptr);
//         vmaDestroyImage(_vmaAllocator, _glbImages[i], _glbImageAllocation[i]);
//     }

//     // shader data
//     vkDestroyDescriptorPool(_logicalDevice, _descriptorSetPool, nullptr);
//     for (const auto &descriptorSetLayout : _descriptorSetLayouts)
//     {
//         vkDestroyDescriptorSetLayout(_logicalDevice, descriptorSetLayout, nullptr);
//     }

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
//     {
//         // vma buffer
//         // unmap a buffer not mapped will crash
//         // vmaUnmapMemory(_vmaAllocator, _vmaAllocations[i]);
//         vmaDestroyBuffer(_vmaAllocator, _uniformBuffers[i], _vmaAllocations[i]);
//         // sync
//         vkDestroySemaphore(_logicalDevice, _imageCanAcquireSemaphores[i], nullptr);
//         vkDestroySemaphore(_logicalDevice, _imageRendereredSemaphores[i], nullptr);
//         vkDestroyFence(_logicalDevice, _inFlightFences[i], nullptr);
//     }

//     vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);
//     vkDestroyPipeline(_logicalDevice, _graphicsPipeline, nullptr);
//     vkDestroyPipelineLayout(_logicalDevice, _pipelineLayout, nullptr);
//     vkDestroyRenderPass(_logicalDevice, _swapChainRenderPass, nullptr);

//     vmaDestroyAllocator(_vmaAllocator);

//     vkDestroyDevice(_logicalDevice, nullptr);
//     vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
//     vkDestroySurfaceKHR(_instance, _surface, nullptr);
//     vkDestroyInstance(_instance, nullptr);
//     _initialized = false;
// }

// void VkApplication::renderPerFrame()
// {
//     // no timeout set
//     VK_CHECK(vkWaitForFences(_logicalDevice, 1, &_inFlightFences[_currentFrameId], VK_TRUE,
//                              UINT64_MAX));
//     // VK_CHECK(vkResetFences(device_, 1, &acquireFence_));
//     uint32_t swapChainImageIndex;
//     VkResult result = vkAcquireNextImageKHR(
//         _logicalDevice, _swapChain, UINT64_MAX, _imageCanAcquireSemaphores[_currentFrameId],
//         VK_NULL_HANDLE, &swapChainImageIndex);
//     if (result == VK_ERROR_OUT_OF_DATE_KHR)
//     {
//         recreateSwapChain();
//         return;
//     }
//     assert(result == VK_SUCCESS ||
//            result == VK_SUBOPTIMAL_KHR); // failed to acquire swap chain image
//     updateUniformBuffer(_currentFrameId);

//     // vkWaitForFences and reset pattern
//     VK_CHECK(vkResetFences(_logicalDevice, 1, &_inFlightFences[_currentFrameId]));
//     // vkWaitForFences ensure the previous command is submitted from the host, now it can be modified.
//     VK_CHECK(vkResetCommandBuffer(_commandBuffers[_currentFrameId], 0));

//     recordCommandBuffer(_commandBuffers[_currentFrameId], swapChainImageIndex);
//     // submit command
//     VkSubmitInfo submitInfo{};
//     submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

//     VkSemaphore waitSemaphores[] = {_imageCanAcquireSemaphores[_currentFrameId]};
//     // pipeline stages
//     // specifies the stage of the pipeline after blending where the final color values are output from the pipeline
//     // basically wait for the previous rendering finished
//     VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

//     submitInfo.waitSemaphoreCount = 1;
//     submitInfo.pWaitSemaphores = waitSemaphores;
//     submitInfo.pWaitDstStageMask = waitStages;
//     submitInfo.commandBufferCount = 1;
//     submitInfo.pCommandBuffers = &_commandBuffers[_currentFrameId];
//     // signal semaphore
//     VkSemaphore signalRenderedSemaphores[] = {_imageRendereredSemaphores[_currentFrameId]};
//     submitInfo.signalSemaphoreCount = 1;
//     submitInfo.pSignalSemaphores = signalRenderedSemaphores;
//     // signal fence
//     VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrameId]));

//     // present after rendering is done
//     VkPresentInfoKHR presentInfo{};
//     presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//     presentInfo.waitSemaphoreCount = 1;
//     presentInfo.pWaitSemaphores = signalRenderedSemaphores;

//     VkSwapchainKHR swapChains[] = {_swapChain};
//     presentInfo.swapchainCount = 1;
//     presentInfo.pSwapchains = swapChains;
//     presentInfo.pImageIndices = &swapChainImageIndex;
//     presentInfo.pResults = nullptr;
//     VK_CHECK(vkQueuePresentKHR(_presentationQueue, &presentInfo));

//     _currentFrameId = (_currentFrameId + 1) % MAX_FRAMES_IN_FLIGHT;
// }

void VkApplication::createInstance()
{
    log(Level::Info, "createInstance");

    assert(!_enableValidationLayers ||
           checkValidationLayerSupport()); // validation layers requested, but
    // not available!

    // non-sdl2 extension supported
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                           extensions.data());
    log(Level::Info, "Found ", extensionCount, " available Instance Extension(s)");
    for (const auto &extension : extensions)
    {
        log(Level::Info, extension.extensionName);
    }

    // SDL2 specific extension supported
    {
        unsigned int extensionCount = 0;
        SDL_Vulkan_GetInstanceExtensions(_window.nativeHandle(), &extensionCount, nullptr);
        std::vector<const char *> extensions(extensionCount);
        SDL_Vulkan_GetInstanceExtensions(_window.nativeHandle(), &extensionCount, extensions.data());
        log(Level::Info, "SDL2 Found ", extensionCount, " available Instance Extension(s)");
        for (const auto &extension : extensions)
        {
            log(Level::Info, extension);
        }
    }

    // 6. VkApplicationInfo
    VkApplicationInfo applicationInfo;
    {
        applicationInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "TestVulkan",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_3,
        };
    }

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo;
    {
        messengerInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = &debugCallback,
            .pUserData = nullptr,
        };
    }

#ifndef __ANDROID__
    // 4. VkLayerSettingsCreateInfoEXT - Specify layer capabilities for a Vulkan instance
    VkLayerSettingsCreateInfoEXT layer_settings_create_info;
    {
        const std::string layer_name = "VK_LAYER_KHRONOS_validation";
        const std::array<const char *, 1> setting_debug_action = {"VK_DBG_LAYER_ACTION_BREAK"};
        const std::array<const char *, 1> setting_gpu_based_action = {
            "GPU_BASED_DEBUG_PRINTF"};
        const std::array<VkBool32, 1> setting_printf_to_stdout = {VK_TRUE};
        const std::array<VkBool32, 1> setting_printf_verbose = {VK_TRUE};
        const std::array<VkLayerSettingEXT, 4> settings = {
            VkLayerSettingEXT{
                .pLayerName = layer_name.c_str(),
                .pSettingName = "debug_action",
                .type = VK_LAYER_SETTING_TYPE_STRING_EXT,
                .valueCount = 1,
                .pValues = setting_debug_action.data(),
            },
            VkLayerSettingEXT{
                .pLayerName = layer_name.c_str(),
                .pSettingName = "validate_gpu_based",
                .type = VK_LAYER_SETTING_TYPE_STRING_EXT,
                .valueCount = 1,
                .pValues = setting_gpu_based_action.data(),
            },
            VkLayerSettingEXT{
                .pLayerName = layer_name.c_str(),
                .pSettingName = "printf_to_stdout",
                .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
                .valueCount = 1,
                .pValues = setting_printf_to_stdout.data(),
            },
            VkLayerSettingEXT{
                .pLayerName = layer_name.c_str(),
                .pSettingName = "printf_verbose",
                .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
                .valueCount = 1,
                .pValues = setting_printf_verbose.data(),
            },
        };

        layer_settings_create_info = {
            .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
            .pNext = &messengerInfo,
            .settingCount = static_cast<uint32_t>(settings.size()),
            .pSettings = settings.data(),
        };
    }
#endif

    // 5. VkValidationFeaturesEXT - Specify validation features to enable or disable for a Vulkan instance
    VkValidationFeaturesEXT validationFeatures;

    // VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT: specifies that the layers will process debugPrintfEXT operations in shaders and send the resulting output to the debug callback.
    // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT: specifies that GPU-assisted validation is enabled. Activating this feature instruments shader programs to generate additional diagnostic data. This feature is disabled by default
    std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnabled{
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
    // linked list
    validationFeatures = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .pNext = &layer_settings_create_info,
        .enabledValidationFeatureCount = static_cast<uint32_t>(validationFeaturesEnabled.size()),
        .pEnabledValidationFeatures = validationFeaturesEnabled.data(),
    };

#ifdef __ANDROID__
    validationFeatures.pNext = &messengerInfo;
#endif

    //     // debug report extension is deprecated
    //     // The window must have been created with the SDL_WINDOW_VULKAN flag and instance must have been created
    //     // with extensions returned by SDL_Vulkan_GetInstanceExtensions() enabled.
    //     std::vector<const char *> instanceExtensions{
    //         VK_KHR_SURFACE_EXTENSION_NAME,
    // #ifdef __ANDROID__
    //         "VK_KHR_android_surface",
    // #endif
    // #ifdef _WIN64
    //         VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    // #endif _
    //             VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    //         VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    //         // shader printf
    //         // VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    //     };

    const auto instanceExtensionSets = getInstanceExtensions();
    std::vector<const char *> instanceExtensions;
    std::transform(instanceExtensionSets.begin(), instanceExtensionSets.end(),
                   std::back_inserter(instanceExtensions),
                   [](const std::string &instanceExtension)
                   {
                       return instanceExtension.c_str();
                   });

    log(Level::Info, "Enable ", instanceExtensions.size(), " available Instance Extension(s)");
    for (const auto &extension : instanceExtensions)
    {
        log(Level::Info, extension);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    if (_enableValidationLayers)
    {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(_validationLayers.size());
        createInfo.ppEnabledLayerNames = _validationLayers.data();
        createInfo.pNext = &validationFeatures;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &_instance));
    volkLoadInstance(_instance);
    // 8. Create Debug Utils Messenger
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(_instance, &messengerInfo, nullptr,
                                            &_debugMessenger));
    ASSERT(_debugMessenger != VK_NULL_HANDLE, "Error creating DebugUtilsMessenger");
};

void VkApplication::createSurface()
{
#if defined(__ANDROID__)
    ASSERT(_osWindow, "_osWindow is needed to create os surface");
    const VkAndroidSurfaceCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .window = _osWindow.get()};

    VK_CHECK(vkCreateAndroidSurfaceKHR(_instance, &create_info, nullptr, &_surface));
#endif

#ifndef __ANDROID__
    // Caution:
    // The window must have been created with the SDL_WINDOW_VULKAN flag and instance must have been created
    // with extensions returned by SDL_Vulkan_GetInstanceExtensions() enabled.
    ASSERT(_window.nativeHandle(), "SDL_window is needed to create os surface");
    auto sdlWindow = _window.nativeHandle();
    SDL_Vulkan_CreateSurface(sdlWindow, _instance, &_surface);
    ASSERT(_surface != VK_NULL_HANDLE, "Error creating SDL_Vulkan_CreateSurface");
#endif
}

void VkApplication::selectPhysicalDevice()
{
    // 10. Select Physical Device based on surface
    {
        //  {VK_KHR_SWAPCHAIN_EXTENSION_NAME},  // physical device extensions
        uint32_t physicalDeviceCount{0};
        VK_CHECK(vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr));
        ASSERT(physicalDeviceCount > 0, "No Vulkan Physical Devices found");
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount,
                                            physicalDevices.data()));
        log(Level::Info, "Found ", physicalDeviceCount, "Vulkan capable device(s)");

        // select physical gpu
        VkPhysicalDevice discrete_gpu = VK_NULL_HANDLE;
        VkPhysicalDevice integrated_gpu = VK_NULL_HANDLE;

        VkPhysicalDeviceProperties prop;
        for (uint32_t i = 0; i < physicalDeviceCount; ++i)
        {
            VkPhysicalDevice physicalDevice = physicalDevices[i];
            vkGetPhysicalDeviceProperties(physicalDevice, &prop);

            if (prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                uint32_t queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                                         nullptr);

                std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                                         queueFamilies.data());

                VkBool32 surfaceSupported;
                for (uint32_t familyIndex = 0; familyIndex < queueFamilyCount; ++familyIndex)
                {
                    const VkQueueFamilyProperties &queueFamilyProp = queueFamilies[familyIndex];
                    // graphics or GPGPU family queue
                    if (queueFamilyProp.queueCount > 0 && (queueFamilyProp.queueFlags &
                                                           (VK_QUEUE_GRAPHICS_BIT |
                                                            VK_QUEUE_COMPUTE_BIT)))
                    {
                        vkGetPhysicalDeviceSurfaceSupportKHR(
                            physicalDevice,
                            familyIndex,
                            _surface,
                            &surfaceSupported);

                        if (surfaceSupported)
                        {
                            _presentQueueFamilyIndex = familyIndex;
                            discrete_gpu = physicalDevice;
                            break;
                        }
                    }
                }
                continue;
            }

            if (prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                uint32_t queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                                         nullptr);

                std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                                         queueFamilies.data());

                VkBool32 surfaceSupported;
                for (uint32_t familyIndex = 0; familyIndex < queueFamilyCount; ++familyIndex)
                {
                    const VkQueueFamilyProperties &queueFamilyProp = queueFamilies[familyIndex];
                    // graphics or GPGPU family queue
                    if (queueFamilyProp.queueCount > 0 && (queueFamilyProp.queueFlags &
                                                           (VK_QUEUE_GRAPHICS_BIT |
                                                            VK_QUEUE_COMPUTE_BIT)))
                    {
                        vkGetPhysicalDeviceSurfaceSupportKHR(
                            physicalDevice,
                            familyIndex,
                            _surface,
                            &surfaceSupported);

                        if (surfaceSupported)
                        {
                            _presentQueueFamilyIndex = familyIndex;
                            integrated_gpu = physicalDevice;

                            break;
                        }
                    }
                }
                continue;
            }
        }

        _selectedPhysicalDevice = discrete_gpu ? discrete_gpu : integrated_gpu;
        ASSERT(_selectedPhysicalDevice, "No Vulkan Physical Devices found");
        ASSERT(_presentQueueFamilyIndex != std::numeric_limits<uint32_t>::max(),
               "No Queue Family Index supporting surface found");
    }
}

void VkApplication::queryPhysicalDeviceCaps()
{
    // 11. Query and Logging physical device (if some feature not supported by the physical device,
    // then we cannot enable them when we create the logic device later on)
    {
        // check if the descriptor index(bindless) is supported
        // Query bindless extension, called Descriptor Indexing (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_descriptor_indexing.html)
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES, nullptr};
        VkPhysicalDeviceFeatures2 deviceFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                                 &indexingFeatures};
        vkGetPhysicalDeviceFeatures2(_selectedPhysicalDevice, &deviceFeatures);
        _bindlessSupported = indexingFeatures.descriptorBindingPartiallyBound &&
                             indexingFeatures.runtimeDescriptorArray;
        ASSERT(_bindlessSupported, "Bindless is not supported");

        // Properties V1
        vkGetPhysicalDeviceProperties(_selectedPhysicalDevice, &_physicalDevicesProp1);
        // query device limits
        // timestampPeriod is the number of nanoseconds required for a timestamp query to be incremented by 1.
        // See Timestamp Queries.
        // auto gpu_timestamp_frequency = physicalDevicesProp1.limits.timestampPeriod / (1000 * 1000);
        // auto s_ubo_alignment = vulkan_physical_properties.limits.minUniformBufferOffsetAlignment;
        // auto s_ssbo_alignemnt = vulkan_physical_properties.limits.minStorageBufferOffsetAlignment;

        log(Level::Info,
            " GPU Used: ", _physicalDevicesProp1.deviceName,
            " Vendor: ", _physicalDevicesProp1.vendorID,
            " Device: ", _physicalDevicesProp1.deviceID);

        // If the VkPhysicalDeviceSubgroupProperties structure is included in the pNext chain of the VkPhysicalDeviceProperties2 structure passed to vkGetPhysicalDeviceProperties2,
        // it is filled in with each corresponding implementation-dependent property.
        // linked-list
        VkPhysicalDeviceSubgroupProperties subgroupProp{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        subgroupProp.pNext = NULL;
        VkPhysicalDeviceProperties2 physicalDevicesProp{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        physicalDevicesProp.pNext = &subgroupProp;
        vkGetPhysicalDeviceProperties2(_selectedPhysicalDevice, &physicalDevicesProp);

        // Get memory properties
        VkPhysicalDeviceMemoryProperties2 memoryProperties_ = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        };
        vkGetPhysicalDeviceMemoryProperties2(_selectedPhysicalDevice, &memoryProperties_);

        // extensions for this selected device
        uint32_t extensionPropertyCount{0};
        VK_CHECK(vkEnumerateDeviceExtensionProperties(_selectedPhysicalDevice, nullptr,
                                                      &extensionPropertyCount, nullptr));
        std::vector<VkExtensionProperties> extensionProperties(extensionPropertyCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(_selectedPhysicalDevice, nullptr,
                                                      &extensionPropertyCount,
                                                      extensionProperties.data()));
        // convert to c++ string
        std::vector<std::string> extensions;
        std::transform(extensionProperties.begin(), extensionProperties.end(),
                       std::back_inserter(extensions),
                       [](const VkExtensionProperties &property)
                       {
                           return std::string(property.extensionName);
                       });
        log(Level::Info, "Found ", extensions.size(), " physical device extension(s)");
        for (const auto &ext : extensions)
        {
            log(Level::Info, ext);
        }
    }
}

void VkApplication::selectQueueFamily()
{
    // 12. Query the selected device to cache the device queue family
    // 1th of main family or 0th of only compute family

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_selectedPhysicalDevice, &queueFamilyCount,
                                             nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_selectedPhysicalDevice, &queueFamilyCount,
                                             queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        auto &queueFamily = queueFamilies[i];
        if (queueFamily.queueCount == 0)
        {
            continue;
        }

        log(Level::Info,
            " Queue Family Index: ", i,
            " Queue Flags: ", queueFamily.queueFlags,
            " Queue Count: ", queueFamily.queueCount);

        // |: means or, both graphics and compute
        if ((queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            // Sparse memory bindings execute on a queue that includes the VK_QUEUE_SPARSE_BINDING_BIT bit
            // While some implementations may include VK_QUEUE_SPARSE_BINDING_BIT support in queue families that also include graphics and compute support
            ASSERT((queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ==
                       VK_QUEUE_SPARSE_BINDING_BIT,
                   "Sparse memory bindings is not supported");
            _graphicsComputeQueueFamilyIndex = i;
            _graphicsQueueIndex = 0;
            // separate graphics and compute queue
            if (queueFamily.queueCount > 1)
            {
                _computeQueueFamilyIndex = i;
                _computeQueueIndex = 1;
            }
            continue;
        }
        // compute only
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            _computeQueueIndex == std::numeric_limits<uint32_t>::max())
        {
            _computeQueueFamilyIndex = i;
            _computeQueueIndex = 0;
        }
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 &&
            (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            _transferQueueFamilyIndex = i;
            continue;
        }
    }
}

VkPhysicalDeviceFeatures VkApplication::sPhysicalDeviceFeatures = {

};

VkPhysicalDeviceFeatures2 VkApplication::sPhysicalDeviceFeatures2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .features = sPhysicalDeviceFeatures,
};

VkPhysicalDeviceVulkan11Features VkApplication::sEnable11Features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
};

VkPhysicalDeviceVulkan12Features VkApplication::sEnable12Features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
};

VkPhysicalDeviceVulkan13Features VkApplication::sEnable13Features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
};

VkPhysicalDeviceAccelerationStructureFeaturesKHR VkApplication::sAccelStructFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
};

VkPhysicalDeviceRayTracingPipelineFeaturesKHR VkApplication::sRayTracingPipelineFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
};

VkPhysicalDeviceRayQueryFeaturesKHR VkApplication::sRayQueryFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
};

VkPhysicalDeviceFragmentDensityMapFeaturesEXT VkApplication::sFragmentDensityMapFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
};

void VkApplication::selectFeatures()
{
    // query all features through single linked list
    // physicalFeatures2 --> indexing_features --> dynamicRenderingFeatures --> nullptr;
    // this gets the capabilities
    vkGetPhysicalDeviceFeatures2(_selectedPhysicalDevice, &_physicalFeatures2);

    // enable features
    sPhysicalDeviceFeatures.independentBlend = VK_TRUE;
    sPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
    sPhysicalDeviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
    sPhysicalDeviceFeatures.multiDrawIndirect = VK_TRUE;
    sPhysicalDeviceFeatures.drawIndirectFirstInstance = VK_TRUE;
    sPhysicalDeviceFeatures.independentBlend = VK_TRUE;

    if (_vk11features.shaderDrawParameters)
    {
        sEnable11Features.shaderDrawParameters = VK_TRUE;
    }

    sEnable11Features.storageBuffer16BitAccess = VK_TRUE;

    sEnable12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    sEnable12Features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    sEnable12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    sEnable12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    sEnable12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    sEnable12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    sEnable12Features.descriptorBindingPartiallyBound = VK_TRUE;
    sEnable12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    sEnable12Features.descriptorIndexing = VK_TRUE;
    sEnable12Features.runtimeDescriptorArray = VK_TRUE;
    sEnable12Features.scalarBlockLayout = VK_TRUE;
    sEnable12Features.bufferDeviceAddress = VK_TRUE;
    sEnable12Features.bufferDeviceAddressCaptureReplay = VK_TRUE;
    sEnable12Features.drawIndirectCount = VK_TRUE;
    sEnable12Features.shaderFloat16 = VK_TRUE;

    sEnable13Features.dynamicRendering = VK_TRUE;
    sEnable13Features.maintenance4 = VK_TRUE;
    sEnable13Features.synchronization2 = VK_TRUE;

    // now is to toggle features selectively
    // pointer chain: life cycle of those pointers must be static
    _featureChain.push(sPhysicalDeviceFeatures2);
    _featureChain.push(sEnable11Features);
    _featureChain.push(sEnable12Features);
    _featureChain.push(sEnable13Features);

    if (checkRayTracingSupport())
    {
        sAccelStructFeatures.accelerationStructure = VK_TRUE;
        sRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        sRayQueryFeatures.rayQuery = VK_TRUE;
        _featureChain.push(sAccelStructFeatures);
        _featureChain.push(sRayTracingPipelineFeatures);
        _featureChain.push(sRayQueryFeatures);
    }

    if (isMultiviewSupported())
    {
        // static, it is okay to modify after push to the chain
        sEnable11Features.multiview = true;
    }

    if (isFragmentDensityMapSupported())
    {
        // static, it is okay to modify after push to the chain
        sFragmentDensityMapFeatures.fragmentDensityMap = true;
        _featureChain.push(sFragmentDensityMapFeatures);
    }
}

void VkApplication::createLogicDevice()
{
    // enable 3 queue family for the logic device (compute/graphics/transfer)
    const float queuePriority[] = {1.0f, 1.0f};
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    uint32_t queueCount = 0;
    VkDeviceQueueCreateInfo graphicsComputeQueue;
    graphicsComputeQueue.pNext = nullptr;
    graphicsComputeQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    // https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/protected.adoc
    // must enable physical device feature
    // graphicsComputeQueue.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
    graphicsComputeQueue.flags = 0x0;
    graphicsComputeQueue.queueFamilyIndex = _graphicsComputeQueueFamilyIndex;
    // within that queuefamily:[0(graphics), 1(compute)];
    graphicsComputeQueue.queueCount = (_graphicsComputeQueueFamilyIndex == _computeQueueFamilyIndex
                                           ? 2
                                           : 1);
    graphicsComputeQueue.pQueuePriorities = queuePriority;
    queueInfos.push_back(graphicsComputeQueue);
    // compute in different queueFamily
    if (_graphicsComputeQueueFamilyIndex != _computeQueueFamilyIndex)
    {
        VkDeviceQueueCreateInfo computeOnlyQueue;
        computeOnlyQueue.pNext = nullptr;
        computeOnlyQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        computeOnlyQueue.flags = 0x0;
        computeOnlyQueue.queueFamilyIndex = _computeQueueFamilyIndex;
        computeOnlyQueue.queueCount = 1;
        computeOnlyQueue.pQueuePriorities = queuePriority; // only the first float will be used (c-style)
        queueInfos.push_back(computeOnlyQueue);
    }

    if (_transferQueueFamilyIndex != std::numeric_limits<uint32_t>::max())
    {
        VkDeviceQueueCreateInfo transferOnlyQueue;
        transferOnlyQueue.pNext = nullptr;
        transferOnlyQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        transferOnlyQueue.flags = 0x0;
        transferOnlyQueue.queueFamilyIndex = _transferQueueFamilyIndex;
        transferOnlyQueue.queueCount = 1;
        const float queuePriority[] = {1.0f};
        transferOnlyQueue.pQueuePriorities = queuePriority;
        // crash the logic device creation
        queueInfos.push_back(transferOnlyQueue);
    }

    // descriptor_indexing
    // Descriptor indexing is also known by the term "bindless",
    // https://docs.vulkan.org/samples/latest/samples/extensions/descriptor_indexing/README.html
    VkDeviceCreateInfo logicDeviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    logicDeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    logicDeviceCreateInfo.pQueueCreateInfos = queueInfos.data();
    logicDeviceCreateInfo.enabledExtensionCount = _deviceExtensions.size();
    logicDeviceCreateInfo.ppEnabledExtensionNames = _deviceExtensions.data();
    logicDeviceCreateInfo.enabledLayerCount =
        static_cast<uint32_t>(_validationLayers.size());
    logicDeviceCreateInfo.ppEnabledLayerNames = _validationLayers.data();
    logicDeviceCreateInfo.pNext = _featureChain.header();

    VK_CHECK(
        vkCreateDevice(_selectedPhysicalDevice, &logicDeviceCreateInfo, nullptr,
                       &_logicalDevice));
    ASSERT(_logicalDevice, "Failed to create logic device");

    setCorrlationId(_instance, _logicalDevice, VK_OBJECT_TYPE_INSTANCE, "Instance: testVulkan");
    setCorrlationId(_logicalDevice, _logicalDevice, VK_OBJECT_TYPE_DEVICE, "Logic Device");

    volkLoadDevice(_logicalDevice);
}

void VkApplication::cacheCommandQueue()
{
    // 0th queue of that queue family is graphics
    vkGetDeviceQueue(_logicalDevice, _graphicsComputeQueueFamilyIndex, _graphicsQueueIndex,
                     &_graphicsQueue);
    vkGetDeviceQueue(_logicalDevice, _computeQueueFamilyIndex, _computeQueueIndex, &_computeQueue);

    // Get transfer queue if present
    if (_transferQueueFamilyIndex != std::numeric_limits<uint32_t>::max())
    {
        vkGetDeviceQueue(_logicalDevice, _transferQueueFamilyIndex, 0, &_transferQueue);
    }

    // familyIndexSupportSurface
    vkGetDeviceQueue(_logicalDevice, _presentQueueFamilyIndex, 0, &_presentationQueue);
    vkGetDeviceQueue(_logicalDevice, _graphicsComputeQueueFamilyIndex, 0, &_sparseQueues);
    ASSERT(_graphicsQueue, "Failed to access graphics queue");
    ASSERT(_computeQueue, "Failed to access compute queue");
    ASSERT(_transferQueue, "Failed to access transfer queue");
    ASSERT(_presentationQueue, "Failed to access presentation queue");
    ASSERT(_sparseQueues, "Failed to access sparse queue");
}

void VkApplication::createVMA()
{
    // https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator

    const VmaVulkanFunctions vulkanFunctions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
#if VMA_VULKAN_VERSION >= 1001000
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR = vkBindBufferMemory2,
        .vkBindImageMemory2KHR = vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
#endif
#if VMA_VULKAN_VERSION >= 1003000
        .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
#endif
    };

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.physicalDevice = _selectedPhysicalDevice;
    allocatorInfo.device = _logicalDevice;
    allocatorInfo.instance = _instance;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &_vmaAllocator);
    ASSERT(_vmaAllocator, "Failed to create vma allocator");
}

void VkApplication::prepareSwapChainCreation()
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_selectedPhysicalDevice, _surface,
                                              &surfaceCapabilities);
    uint32_t width = surfaceCapabilities.currentExtent.width;
    uint32_t height = surfaceCapabilities.currentExtent.height;
    // surfaceCapabilities.supportedCompositeAlpha
    if (surfaceCapabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
        surfaceCapabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
    {
        // Swap to get identity width and height
        surfaceCapabilities.currentExtent.height = width;
        surfaceCapabilities.currentExtent.width = height;
    }
    _swapChainExtent = surfaceCapabilities.currentExtent;
    _pretransformFlag = surfaceCapabilities.currentTransform;

    log(Level::Info, "Creating swapchain...");
    log(Level::Info, "w: ", _swapChainExtent.width);
    log(Level::Info, "h: ", _swapChainExtent.height);
    log(Level::Info, "minImageCount: ", surfaceCapabilities.minImageCount);

    uint32_t supportedSurfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_selectedPhysicalDevice, _surface,
                                         &supportedSurfaceFormatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> supportedFormats(supportedSurfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_selectedPhysicalDevice, _surface,
                                         &supportedSurfaceFormatCount, supportedFormats.data());
    std::vector<VkFormat> formats;
    std::transform(std::begin(supportedFormats), std::end(supportedFormats),
                   std::back_inserter(formats),
                   [](const VkSurfaceFormatKHR &f)
                   { return f.format; });

    // for gamma correction, non-linear color space
    ASSERT(supportedFormats.end() != std::find_if(supportedFormats.begin(), supportedFormats.end(),
                                                  [&](const VkSurfaceFormatKHR &format)
                                                  {
                                                      return format.format == _swapChainFormat &&
                                                             format.colorSpace == _colorspace;
                                                  }),
           "swapChainFormat is not supported");
}

void VkApplication::createSwapChain()
{
    const bool presentationQueueIsShared =
        _graphicsComputeQueueFamilyIndex == _presentQueueFamilyIndex;
    std::array<uint32_t, 2> familyIndices{_graphicsComputeQueueFamilyIndex,
                                          _presentQueueFamilyIndex};

    VkSwapchainCreateInfoKHR swapchain = {};
    swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain.surface = _surface;
    swapchain.minImageCount = _swapChainImageCount;
    swapchain.imageFormat = _swapChainFormat;
    swapchain.imageColorSpace = _colorspace;
    swapchain.imageExtent = _swapChainExtent;
    swapchain.clipped = VK_TRUE;
    swapchain.imageArrayLayers = 1;
    swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // VK_SHARING_MODE_EXCLUSIVE,
    // VK_SHARING_MODE_CONCURRENT: concurrent access to any range or image subresource from multiple queue families
    swapchain.imageSharingMode = presentationQueueIsShared ? VK_SHARING_MODE_EXCLUSIVE
                                                           : VK_SHARING_MODE_CONCURRENT;
    swapchain.queueFamilyIndexCount = presentationQueueIsShared ? 0u : 2u;
    swapchain.pQueueFamilyIndices = presentationQueueIsShared ? nullptr : familyIndices.data();
    swapchain.preTransform = _pretransformFlag;
    // ignore alpha completely
    // not supported: VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR for this default swapchain surface
    swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // VK_PRESENT_MODE_FIFO_KHR always supported
    // VK_PRESENT_MODE_FIFO_KHR = Hard Vsync
    // This is always supported on Android phones
    swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain.oldSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(_logicalDevice, &swapchain, nullptr, &_swapChain));
    setCorrlationId(_swapChain, _logicalDevice, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "Swapchain");
}

void VkApplication::createSwapChainImageViews()
{
    uint32_t imageCount{0};
    VK_CHECK(vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &imageCount, nullptr));
    std::vector<VkImage> images(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &imageCount, images.data()));
    _swapChainImageViews.resize(imageCount);

    VkImageViewCreateInfo imageView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    // VK_IMAGE_VIEW_TYPE_2D_ARRAY for image array
    imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageView.format = _swapChainFormat;
    // no mipmap
    imageView.subresourceRange.baseMipLevel = 0;
    imageView.subresourceRange.levelCount = 1;
    // no image array
    imageView.subresourceRange.baseArrayLayer = 0;
    imageView.subresourceRange.layerCount = 1;
    imageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //    imageView.components.r = VK_COMPONENT_SWIZZLE_R;
    //    imageView.components.g = VK_COMPONENT_SWIZZLE_G;
    //    imageView.components.b = VK_COMPONENT_SWIZZLE_B;
    //    imageView.components.a = VK_COMPONENT_SWIZZLE_A;
    imageView.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageView.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageView.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageView.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    for (size_t i = 0; i < imageCount; ++i)
    {
        imageView.image = images[i];
        VK_CHECK(vkCreateImageView(_logicalDevice, &imageView, nullptr, &_swapChainImageViews[i]));
        setCorrlationId(_swapChainImageViews[i], _logicalDevice, VK_OBJECT_TYPE_IMAGE_VIEW,
                        "Swap Chain Image view: " + std::to_string(i));
    }
}

void VkApplication::createSwapChainRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swapChainFormat;
    // multi-samples here
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // like tree traversal, enter/exit the node
    // enter the renderpass: clear
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // leave the renderpass: store
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // swap chain is not used for stencil, don't care
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // swap chain is for presentation
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // VkAttachmentReference is for subpass, how subpass could refer to the color attachment
    // here only 1 color attachement, index is 0;
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    // for graphics presentation
    // no depth, stencil and multi-sampling
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // subpass dependencies
    // VK_SUBPASS_EXTERNAL means anything outside of a given render pass scope.
    // When used for srcSubpass it specifies anything that happened before the render pass.
    // And when used for dstSubpass it specifies anything that happens after the render pass.
    // It means that synchronization mechanisms need to include operations that happen before
    // or after the render pass.
    // It may be another render pass, but it also may be some other operations,
    // not necessarily render pass-related.

    // dstSubpass: 0
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0] = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        // specifies all operations performed by all commands
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        // stage of the pipeline after blending
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        // dependencies: memory read may collide with renderpass write
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
    };

    dependencies[1] = {
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
    };

    VkAttachmentDescription attachments[] = {colorAttachment};
    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    // here could set multi-view VkRenderPassMultiviewCreateInfo
    renderPassInfo.pNext = nullptr;
    renderPassInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(_logicalDevice, &renderPassInfo, nullptr, &_swapChainRenderPass));
    setCorrlationId(_swapChainRenderPass, _logicalDevice, VK_OBJECT_TYPE_RENDER_PASS, "Render pass: SwapChain");
}

// void VkApplication::recreateSwapChain()
// {
//     // wait on the host for the completion of outstanding queue operations for all queues
//     vkDeviceWaitIdle(_logicalDevice);
//     deleteSwapChain();
//     createSwapChain();
//     createSwapChainImageViews();
//     createSwapChainFramebuffers();
// }

// void VkApplication::deleteSwapChain()
// {
//     for (size_t i = 0; i < _swapChainFramebuffers.size(); ++i)
//     {
//         vkDestroyFramebuffer(_logicalDevice, _swapChainFramebuffers[i], nullptr);
//     }

//     for (size_t i = 0; i < _swapChainImageViews.size(); i++)
//     {
//         vkDestroyImageView(_logicalDevice, _swapChainImageViews[i], nullptr);
//     }
//     // image is owned by swap chain
//     vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);
// }

// depends on shader, and used by graphicsPipelineDesc
// each set have one instance of layout
void VkApplication::createDescriptorSetLayout()
{
    // be careful of the MAX_FLIGHT
    // set0: one ubo in vs: layout (set = 0, binding = 0) uniform UBO (Yes, has MAX_FLIGHT)
    // set1: one sampler2D in fs: layout (set = 1, binding = 0) uniform sampler2D samplerColor;
    // set2: glb packed buffer: layout(set = 1, binding = 0) readonly buffer VertexBuffer
    // set6: glb material combo buffer

    // Descriptor binding flag VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT:
    // This flag indicates that descriptor set does not need to have valid descriptors in them
    // as long as the invalid descriptors are not accessed during shader execution.

    constexpr VkDescriptorBindingFlags flagsToEnable = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    {
        // for set0: ubo with MAX_FLIGHTS
        std::vector<VkDescriptorSetLayoutBinding> dsLayoutBindings(1);
        dsLayoutBindings[0].binding = 0; // depends on the shader: set 0, binding = 0
        dsLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // array resource
        dsLayoutBindings[0].descriptorCount = 1;
        dsLayoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        dsLayoutBindings[0].pImmutableSamplers = nullptr;

        std::vector<VkDescriptorBindingFlags> bindFlags(dsLayoutBindings.size(), flagsToEnable);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(dsLayoutBindings.size()),
            .pBindingFlags = bindFlags.data(),
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = dsLayoutBindings.size();
        layoutInfo.pBindings = dsLayoutBindings.data();
#if defined(_WIN32)
        layoutInfo.pNext = &extendedInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
#endif
        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr,
                                             &descriptorSetLayout));

        _descriptorSetLayouts.push_back(descriptorSetLayout);
        _descriptorSetLayoutForUbo = descriptorSetLayout;
    }

    {
        VkDescriptorSetLayoutBinding dsLayoutBindings{};
        dsLayoutBindings.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        dsLayoutBindings.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        dsLayoutBindings.binding = 0;
        dsLayoutBindings.descriptorCount = 1;

        const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = 1,
            .pBindingFlags = &flagsToEnable,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &dsLayoutBindings;
#if defined(_WIN32)
        layoutInfo.pNext = &extendedInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
#endif

        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr,
                                             &descriptorSetLayout));

        _descriptorSetLayouts.push_back(descriptorSetLayout);
        _descriptorSetLayoutForComboVertexBuffer = descriptorSetLayout;
    }

    {
        // set3 ssbo: for glb indirectDrawBuffer
        VkDescriptorSetLayoutBinding dsLayoutBindings{};
        dsLayoutBindings.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        dsLayoutBindings.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        dsLayoutBindings.binding = 0;
        dsLayoutBindings.descriptorCount = 1;

        const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = 1,
            .pBindingFlags = &flagsToEnable,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &dsLayoutBindings;
#if defined(_WIN32)
        layoutInfo.pNext = &extendedInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
#endif

        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr,
                                             &descriptorSetLayout));
        _descriptorSetLayouts.push_back(descriptorSetLayout);
        _descriptorSetLayoutForIndirectDrawBuffer = descriptorSetLayout;
    }

    {
        // set 5 bindless Textures and sampler for glb
        // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER vs VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
        {
            VkDescriptorSetLayoutBinding dsLayoutBinding{};
            dsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            dsLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            dsLayoutBinding.binding = 0;
            dsLayoutBinding.descriptorCount = 1; // _glbImageViews.size();
            setLayoutBindings.emplace_back(dsLayoutBinding);
        }
        {
            VkDescriptorSetLayoutBinding dsLayoutBinding{};
            dsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            dsLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            dsLayoutBinding.binding = 1;
            dsLayoutBinding.descriptorCount = 1;
            setLayoutBindings.emplace_back(dsLayoutBinding);
        }

        std::vector<VkDescriptorBindingFlags> bindFlags(setLayoutBindings.size(), flagsToEnable);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(bindFlags.size()),
            .pBindingFlags = bindFlags.data(),
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = setLayoutBindings.data();
#if defined(_WIN32)
        layoutInfo.pNext = &extendedInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
#endif

        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr,
                                             &descriptorSetLayout));
        _descriptorSetLayouts.push_back(descriptorSetLayout);
        _descriptorSetLayoutForTextureAndSampler = descriptorSetLayout;
    }

    {
        // set5 ssbo: for glb material
        VkDescriptorSetLayoutBinding dsLayoutBindings{};
        dsLayoutBindings.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        dsLayoutBindings.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        dsLayoutBindings.binding = 0;
        dsLayoutBindings.descriptorCount = 1;

        const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = 1,
            .pBindingFlags = &flagsToEnable,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &dsLayoutBindings;
#if defined(_WIN32)
        layoutInfo.pNext = &extendedInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
#endif

        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr,
                                             &descriptorSetLayout));
        _descriptorSetLayouts.push_back(descriptorSetLayout);
        _descriptorSetLayoutForComboMaterialBuffer = descriptorSetLayout;
    }
}

// depends on your glsl
void VkApplication::createDescriptorPool()
{
    // here I need 4 type of descriptors
    // layout (set = 0, binding = 0) uniform UBO
    // layout (set = 1, binding = 0) readonly buffer VertexBuffer;
    // layout (set = 2, binding = 0) readonly buffer IndirectDraw;
    // layout(set = 3, binding = 0) uniform texture2D BindlessImage2D[];
    // layout(set = 3, binding = 1) uniform sampler BindlessSampler[];
    // layout (set = 4, binding = 0) readonly buffer MaterialBuffer;

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes(4);
    // poolSize.descriptorCount = static_cast<uint32_t>(MAX_DESCRIPTOR_SETS);
    descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolSizes[0].descriptorCount = 1 * MAX_FRAMES_IN_FLIGHT;
    descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorPoolSizes[1].descriptorCount = 10;
    descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorPoolSizes[2].descriptorCount = 10;
    descriptorPoolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorPoolSizes[3].descriptorCount = 10;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                     VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    poolInfo.poolSizeCount = descriptorPoolSizes.size();
    poolInfo.pPoolSizes = descriptorPoolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(100);

    VK_CHECK(vkCreateDescriptorPool(_logicalDevice, &poolInfo, nullptr, &_descriptorSetPool));
}

void VkApplication::allocateDescriptorSets()
{
    ASSERT(_descriptorSetLayouts.size() == 5,
           "DS Layouts: ubo | ssbo (vb) | ssbo(indirectDrawBuffer)"
           "| texture2d+sampler | ssbo(mat)");
    // how many ds to allocate ?
    {
        // 1. ubo has MAX_FRAMES_IN_FLIGHT
        _descriptorSetsForUbo.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            // std::vector <VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, _descriptorSetLayout);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = _descriptorSetPool;
            // 3 ds for ubo
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &_descriptorSetLayoutForUbo;
            // VK_ERROR_OUT_OF_POOL_MEMORY_KHR = VK_ERROR_OUT_OF_POOL_MEMORY = -1000069000
            VK_CHECK(
                vkAllocateDescriptorSets(_logicalDevice, &allocInfo,
                                         &_descriptorSetsForUbo[i]));
        }
    }

    {
        // 2. ssbo for vb
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorSetPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &_descriptorSetLayoutForComboVertexBuffer;

        VK_CHECK(
            vkAllocateDescriptorSets(_logicalDevice, &allocInfo,
                                     &_descriptorSetsForGlbSSBO));
    }

    {
        // 3. ssbo for indirectDraw
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorSetPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &_descriptorSetLayoutForIndirectDrawBuffer;

        VK_CHECK(
            vkAllocateDescriptorSets(_logicalDevice, &allocInfo,
                                     &_descriptorSetsForIndirectDrawBuffer));
    }

    {
        // 3. texture2d + sampler
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorSetPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &_descriptorSetLayoutForTextureAndSampler;

        VK_CHECK(
            vkAllocateDescriptorSets(_logicalDevice, &allocInfo,
                                     &_descriptorSetsForTextureAndSampler));
    }

    {
        // 4. ssbo for materials
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorSetPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &_descriptorSetLayoutForComboMaterialBuffer;

        VK_CHECK(
            vkAllocateDescriptorSets(_logicalDevice, &allocInfo,
                                     &_descriptorSetsForMaterialBuffer));
    }
}

// vma
void VkApplication::createPersistentBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    const std::string &name,
    VkBuffer &buffer,
    VmaAllocation &vmaAllocation,
    VmaAllocationInfo &vmaAllocationInfo)
{
    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo vmaAllocationCreateInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        .requiredFlags = properties,
    };
    VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &vmaAllocationCreateInfo, &buffer,
                             &vmaAllocation,
                             nullptr));
    vmaGetAllocationInfo(_vmaAllocator, vmaAllocation, &vmaAllocationInfo);
    setCorrlationId(buffer, _logicalDevice, VK_OBJECT_TYPE_BUFFER, "Persistent Buffer: " + name);
}

void VkApplication::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformDataDef1);
    _uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    _vmaAllocations.resize(MAX_FRAMES_IN_FLIGHT);
    _vmaAllocationInfos.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        createPersistentBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                   VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                               "Uniform buffer " + std::to_string(i),
                               _uniformBuffers[i],
                               _vmaAllocations[i],
                               _vmaAllocationInfos[i]);
    }
}

// /*
//  * getPrerotationMatrix handles screen rotation with 3 hardcoded rotation
//  * matrices (detailed below). We skip the 180 degrees rotation.
//  */
// void getPrerotationMatrix(const VkSurfaceTransformFlagBitsKHR &pretransformFlag,
//                           std::array<float, 16> &mat)
// {
//     // mat is initialized to the identity matrix
//     mat = {1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.};
//     if (pretransformFlag & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
//     {
//         // mat is set to a 90 deg rotation matrix
//         mat = {0., 1., 0., 0., -1., 0, 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.};
//     }
//     else if (pretransformFlag & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
//     {
//         // mat is set to 270 deg rotation matrix
//         mat = {0., -1., 0., 0., 1., 0, 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.};
//     }
// }

// void VkApplication::updateUniformBuffer(int currentFrameId)
// {
//     //    UniformDataDef0 ubo{};
//     //    getPrerotationMatrix(_pretransformFlag, ubo.mvp);

//     auto view = _camera.viewTransformLH();
//     auto persPrj = PerspectiveProjectionTransformLH(0.0001f, 200.0f, 0.3f,
//                                                     (float)_swapChainExtent.width /
//                                                         (float)_swapChainExtent.height);

//     mat4x4f identity(1.0f);
//     auto mv = MatrixMultiply4x4(identity, view);
//     auto vp = MatrixMultiply4x4(view, persPrj);
//     auto mvp = MatrixMultiply4x4(identity, vp);

//     UniformDataDef1 ubo;
//     ubo.viewPos = _camera.viewPos();
//     ubo.modelView = mv;
//     ubo.projection = persPrj;
//     ubo.mvp = mvp;

//     void *mappedMemory{nullptr};
//     VK_CHECK(vmaMapMemory(_vmaAllocator, _vmaAllocations[currentFrameId], &mappedMemory));
//     memcpy(mappedMemory, &ubo, sizeof(UniformDataDef1));
//     // memcpy(mappedMemory, &ubo, sizeof(ubo));
//     vmaUnmapMemory(_vmaAllocator, _vmaAllocations[currentFrameId]);
// }

// void VkApplication::bindResourceToDescriptorSets()
// {
//     // for ubo
//     ASSERT(_descriptorSetsForUbo.size() == MAX_FRAMES_IN_FLIGHT,
//            "ubo descriptor set has frame_in_flight");
//     // extra: 1. texture+sampler, 2. ssbo for vb, 3. ssbo for indirectdraw
//     // 4. textures,
//     // 5. samplers
//     // 6. ssbo for materials
//     uint32_t writeDescriptorSetCount{MAX_FRAMES_IN_FLIGHT + 6};
//     _writeDescriptorSetBundle.reserve(writeDescriptorSetCount);

//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
//     {
//         VkDescriptorBufferInfo bufferInfo{};
//         bufferInfo.buffer = _uniformBuffers[i];
//         bufferInfo.offset = 0;
//         bufferInfo.range = sizeof(UniformDataDef1);

//         _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//             .dstSet = _descriptorSetsForUbo[i],
//             .dstBinding = 0,
//             .dstArrayElement = 0,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//             .pImageInfo = nullptr,
//             .pBufferInfo = &bufferInfo,
//             .pTexelBufferView = VK_NULL_HANDLE,
//         });
//     }

//     // for texture + sampler
//     {
//         VkDescriptorImageInfo imageInfo{};
//         // images are never directly accessed by the shader
//         // imageInfo.imageView = _imageView;
//         imageInfo.imageView = _glbImageViews[0];
//         imageInfo.sampler = _sampler;
//         // The current usage of image: shader read
//         imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

//         _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//             .dstSet = _descriptorSetsForTextureSampler,
//             .dstBinding = 0,
//             .dstArrayElement = 0,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//             .pImageInfo = &imageInfo,
//             .pBufferInfo = nullptr,
//         });
//     }

//     // for glb's vb
//     {
//         VkDescriptorBufferInfo bufferInfo{};
//         bufferInfo.buffer = _compositeVB;
//         bufferInfo.offset = 0;
//         bufferInfo.range = _compositeVBSizeInByte;

//         _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//             .dstSet = _descriptorSetsForGlbSSBO,
//             .dstBinding = 0,
//             .dstArrayElement = 0,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
//             .pImageInfo = nullptr,
//             .pBufferInfo = &bufferInfo,
//         });
//     }

//     // glb indirect draw
//     {
//         VkDescriptorBufferInfo bufferInfo{};
//         bufferInfo.buffer = _indirectDrawB;
//         bufferInfo.offset = 0;
//         bufferInfo.range = _indirectDrawBSizeInByte;

//         _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//             .dstSet = _descriptorSetsForIndirectDrawBuffer,
//             .dstBinding = 0,
//             .dstArrayElement = 0,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
//             .pImageInfo = nullptr,
//             .pBufferInfo = &bufferInfo,
//         });
//     }

//     // for glb textures
//     {
//         VkDescriptorImageInfo imageInfo;
//         imageInfo.sampler = VK_NULL_HANDLE;
//         imageInfo.imageView = _glbImageViews[0];
//         imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

//         // std::vector<VkDescriptorImageInfo> imageInfos;
//         // imageInfos.push_back(imageInfo);

//         _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//             .dstSet = _descriptorSetsForTextureAndSampler,
//             .dstBinding = 1,
//             .dstArrayElement = 0,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
//             .pImageInfo = &imageInfo,
//             .pBufferInfo = nullptr,
//         });

//         //        const auto imageCt = _glbImageViews.size();
//         //        std::vector<VkDescriptorImageInfo> imageInfos;
//         //        imageInfos.reserve(imageCt);
//         //        for (const auto &imageView: _glbImageViews) {
//         //            imageInfos.emplace_back(VkDescriptorImageInfo{
//         //                    .sampler = VK_NULL_HANDLE,
//         //                    .imageView = imageView,
//         //                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//         //            });
//         //        }
//         //
//         //        _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//         //                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//         //                .dstSet = _descriptorSetsForTextureAndSampler,
//         //                .dstBinding = 1,
//         //                .dstArrayElement = 0,
//         //                .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
//         //                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
//         //                .pImageInfo = imageInfos.data(),
//         //                .pBufferInfo = nullptr,
//         //        });
//     }

//     // for glb samplers
//     {
//         VkDescriptorImageInfo samplerInfo;
//         samplerInfo.sampler = _glbSamplers[0];
//         samplerInfo.imageView = VK_NULL_HANDLE;
//         samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         //
//         //        const auto samplersCt = _glbSamplers.size();
//         //        std::vector<VkDescriptorImageInfo> samplerInfos;
//         //        samplerInfos.reserve(samplersCt);
//         //        for (const auto &sampler: _glbSamplers) {
//         //            samplerInfos.emplace_back(VkDescriptorImageInfo{
//         //                    .sampler = sampler,
//         //                    .imageView = VK_NULL_HANDLE,
//         //                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//         //            });
//         //        }
//         _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//             .dstSet = _descriptorSetsForTextureAndSampler,
//             .dstBinding = 2,
//             .dstArrayElement = 0,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
//             .pImageInfo = &samplerInfo,
//             .pBufferInfo = nullptr,
//         });
//     }

//     {
//         VkDescriptorBufferInfo bufferInfo{};
//         bufferInfo.buffer = _compositeMatB;
//         bufferInfo.offset = 0;
//         bufferInfo.range = _compositeMatBSizeInByte;

//         _writeDescriptorSetBundle.emplace_back(VkWriteDescriptorSet{
//             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//             .dstSet = _descriptorSetsForMaterialBuffer,
//             .dstBinding = 0,
//             .dstArrayElement = 0,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
//             .pImageInfo = nullptr,
//             .pBufferInfo = &bufferInfo,
//         });
//     }

//     // Validation Error: [ VUID-VkWriteDescriptorSet-descriptorType-00325 ] Object 0: handle = 0xd10d270000000018, type = VK_OBJECT_TYPE_DESCRIPTOR_SET; Object 1: handle = 0x7fc177270ab3, type = VK_OBJECT_TYPE_SAMPLER; | MessageID = 0xce76343a | vkUpdateDescriptorSets(): pDescriptorWrites[7] Attempted write update to sampler descriptor with invalid sample (VkSampler 0x7fc177270ab3[]).
//     //  The Vulkan spec states: If descriptorType is VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//     //  and dstSet was not allocated with a layout that included immutable samplers for dstBinding with descriptorType, the sampler member of each element of pImageInfo must be a valid VkSampler object (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkWriteDescriptorSet-descriptorType-00325)
//     LOGI("_writeDescriptorSetBundle: %d", _writeDescriptorSetBundle.size());
//     vkUpdateDescriptorSets(_logicalDevice, _writeDescriptorSetBundle.size(),
//                            _writeDescriptorSetBundle.data(), 0,
//                            nullptr);
// }

// // load shader spirv
// std::vector<uint8_t> LoadBinaryFile(const char *file_path,
//                                     AAssetManager *assetManager)
// {
//     std::vector<uint8_t> file_content;
//     assert(assetManager);
//     AAsset *file =
//         AAssetManager_open(assetManager, file_path, AASSET_MODE_BUFFER);
//     size_t file_length = AAsset_getLength(file);

//     file_content.resize(file_length);

//     AAsset_read(file, file_content.data(), file_length);
//     AAsset_close(file);
//     return file_content;
// }

// VkShaderModule createShaderModule(VkDevice logicalDevice, const std::vector<uint8_t> &code)
// {
//     VkShaderModuleCreateInfo createInfo{};
//     createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//     createInfo.codeSize = code.size();
//     createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
//     VkShaderModule shaderModule;
//     VK_CHECK(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule));
//     return shaderModule;
// }

void VkApplication::createShaderModules()
{
    // lateral for filepath in modern c++
    const auto shadersPath = std::filesystem::current_path().parent_path() / "assets";
    const auto vertexShaderPath = shadersPath / "indirectDraw.vert";
    const auto fragShaderPath = shadersPath / "indirectDraw.frag";
    _vsShaderModule = createShaderModule(
        _logicalDevice,
        vertexShaderPath.string(),
        "main",
        "indirectDraw.vert");
    _fsShaderModule = createShaderModule(
        _logicalDevice,
        fragShaderPath.string(),
        "main",
        "indirectDraw.frag");
}

void VkApplication::createGraphicsPipeline()
{
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = _vsShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = _fsShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // vao in opengl, this settings means no vao (vbo), create data in the vs directly

    // without vao rendering. ex: ssbo + vs.
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    // match location with shader
    //    layout (location = 0) in vec3 inPos;
    //    layout (location = 1) in vec2 inUV;
    //    layout (location = 2) in vec3 inNormal;

    VkPipelineVertexInputStateCreateInfo vao{};
    std::vector<VkVertexInputBindingDescription> vertexInputBindings(1);
    vertexInputBindings[0].binding = 0;
    vertexInputBindings[0].stride = sizeof(VertexDef1);
    vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes(3);

    // pos
    vertexInputAttributes[0].location = 0;
    vertexInputAttributes[0].binding = 0;
    vertexInputAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT; // std::array<float, 3> pos
    vertexInputAttributes[0].offset = 0;
    // uv
    vertexInputAttributes[1].location = 1;
    vertexInputAttributes[1].binding = 0;
    vertexInputAttributes[1].format = VK_FORMAT_R32G32_SFLOAT; // std::array<float, 2> uv;
    vertexInputAttributes[1].offset = 3 * sizeof(float);
    // normal
    vertexInputAttributes[2].location = 2;
    vertexInputAttributes[2].binding = 0;
    vertexInputAttributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributes[2].offset = 5 * sizeof(float);

    vao.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vao.vertexBindingDescriptionCount = vertexInputBindings.size();
    vao.pVertexBindingDescriptions = vertexInputBindings.data();
    vao.vertexAttributeDescriptionCount = vertexInputAttributes.size();
    vao.pVertexAttributeDescriptions = vertexInputAttributes.data();

    // for indirect-draw
    vao.vertexBindingDescriptionCount = vao.vertexAttributeDescriptionCount = 0;

    // topology of input data
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // Pipeline Dynamic State for viewport and scissor, not setting it here
    // no hardcode the viewport/scissor options,
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f;          // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // disable alpha blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1; // for MRT
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // only one set layout
    //    // example of two setlayout will be:
    //    layout(set = 0, binding = 0) uniform Transforms
    //    layout(set = 1, binding = 0) uniform ObjectProperties

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // multiple set layouts binded to the graphics pipeline
    pipelineLayoutInfo.setLayoutCount = (uint32_t)_descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = _descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutInfo, nullptr,
                                    &_pipelineLayout));

    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI{};
    dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
    dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    // with vao
    //pipelineInfo.pVertexInputState = &vao;
    // without vao
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = VK_NULL_HANDLE; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateCI;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = _swapChainRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1;              // Optional

    VK_CHECK(vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
                                       nullptr, &_graphicsPipeline));
    // vkDestroyShaderModule(_logicalDevice, fragShaderModule, nullptr);
    // vkDestroyShaderModule(_logicalDevice, vertShaderModule, nullptr);
}

// void VkApplication::createSwapChainFramebuffers()
// {
//     _swapChainFramebuffers.resize(_swapChainImageViews.size());
//     VkFramebufferCreateInfo framebufferInfo{};
//     framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
//     framebufferInfo.renderPass = _swapChainRenderPass;
//     framebufferInfo.attachmentCount = 1;
//     framebufferInfo.width = _swapChainExtent.width;
//     framebufferInfo.height = _swapChainExtent.height;
//     framebufferInfo.layers = 1;
//     for (size_t i = 0; i < _swapChainImageViews.size(); i++)
//     {
//         VkImageView attachments[] = {_swapChainImageViews[i]};
//         framebufferInfo.pAttachments = attachments;
//         VK_CHECK(vkCreateFramebuffer(_logicalDevice, &framebufferInfo, nullptr,
//                                      &_swapChainFramebuffers[i]));
//     }
// }

// void VkApplication::createCommandPool()
// {
//     VkCommandPoolCreateInfo poolInfo{};
//     poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
//     // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
//     // since we want to
//     // record a command
//     // buffer every
//     // frame, so we want
//     // to be able to
//     // reset and record
//     // over it
//     poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
//     poolInfo.queueFamilyIndex = _graphicsComputeQueueFamilyIndex;
//     VK_CHECK(vkCreateCommandPool(_logicalDevice, &poolInfo, nullptr, &_commandPool));
// }

// void VkApplication::createCommandBuffer()
// {
//     _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
//     VkCommandBufferAllocateInfo allocInfo{};
//     allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//     allocInfo.commandPool = _commandPool;
//     allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//     allocInfo.commandBufferCount = _commandBuffers.size();
//     VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _commandBuffers.data()));
// }

// void VkApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t swapChainImageIndex)
// {
//     VkCommandBufferBeginInfo beginInfo{};
//     beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//     //  command buffer will be reset and recorded again between each submissio
//     beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//     beginInfo.pInheritanceInfo = nullptr;

//     VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

//     // Begin Render Pass, only 1 render pass
//     constexpr VkClearValue clearColor{0.0f, 0.0f, 0.0f, 0.0f};
//     VkRenderPassBeginInfo renderPassInfo{};
//     renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//     renderPassInfo.renderPass = _swapChainRenderPass;
//     // fbo corresponding to the swapchain image index
//     renderPassInfo.framebuffer = _swapChainFramebuffers[swapChainImageIndex];
//     renderPassInfo.renderArea.offset = {0, 0};
//     renderPassInfo.renderArea.extent = _swapChainExtent;
//     renderPassInfo.clearValueCount = 1;
//     renderPassInfo.pClearValues = &clearColor;
//     vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

//     // Dynamic States (when create the graphics pipeline, they are not specified)
//     VkViewport viewport{};
//     viewport.width = (float)_swapChainExtent.width;
//     viewport.height = (float)_swapChainExtent.height;
//     viewport.minDepth = 0.0f;
//     viewport.maxDepth = 1.0f;
//     vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

//     VkRect2D scissor{};
//     scissor.extent = _swapChainExtent;
//     vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
//     vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);

//     // apply graphics pipeline to the cmd
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
//     // resource and ds to the shaders of this pipeline
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             _pipelineLayout, 0, 1, &_descriptorSetsForUbo[_currentFrameId],
//                             0, nullptr);
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             _pipelineLayout, 1, 1, &_descriptorSetsForTextureSampler,
//                             0, nullptr);
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             _pipelineLayout, 3, 1, &_descriptorSetsForGlbSSBO,
//                             0, nullptr);
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             _pipelineLayout, 2, 1, &_descriptorSetsForIndirectDrawBuffer,
//                             0, nullptr);
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             _pipelineLayout, 4, 1, &_descriptorSetsForTextureAndSampler,
//                             0, nullptr);
//     //    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//     //                            _pipelineLayout, 5, 1, &_descriptorSetsForSampler,
//     //                            0, nullptr);
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             _pipelineLayout, 5, 1, &_descriptorSetsForMaterialBuffer,
//                             0, nullptr);
//     //    // submit draw call, data is generated in the vs
//     //    // for ssbo + vs, gpu-driven rendering
//     //    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

//     vkCmdBindIndexBuffer(commandBuffer, _compositeIB, 0, VK_INDEX_TYPE_UINT32);
//     // how many draws are dependent on how many meshes in the scene.
//     vkCmdDrawIndexedIndirect(commandBuffer, _indirectDrawB, 0, _numMeshes,
//                              sizeof(IndirectDrawForVulkan));

//     //    // draw for textured quad
//     //    // for vao driven draw
//     //    VkDeviceSize offsets[1] = {0};
//     //    // it could bind multiple VBs, here only need 1.
//     //    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &_deviceVb, offsets);
//     //    // no offset, start from zero
//     //    vkCmdBindIndexBuffer(commandBuffer, _deviceIb, 0, VK_INDEX_TYPE_UINT32);
//     //
//     //    const uint32_t instanceCt = 1;
//     //    const uint32_t firstIndex = 0;
//     //    const uint32_t vertexOffset = 0;
//     //    const uint32_t firstInstance = 0;
//     //    vkCmdDrawIndexed(commandBuffer, _indexCount, instanceCt, firstIndex, vertexOffset,
//     //                     firstInstance);
//     vkCmdEndRenderPass(commandBuffer);
//     VK_CHECK(vkEndCommandBuffer(commandBuffer));
// }

// void VkApplication::createPerFrameSyncObjects()
// {
//     _imageCanAcquireSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
//     _imageRendereredSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
//     _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

//     VkSemaphoreCreateInfo semaphoreInfo{};
//     semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

//     VkFenceCreateInfo fenceInfo{};
//     fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
//     fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
//     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
//     {
//         VK_CHECK(vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr,
//                                    &_imageCanAcquireSemaphores[i]));
//         VK_CHECK(vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr,
//                                    &_imageRendereredSemaphores[i]));
//         VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_inFlightFences[i]));
//     }
// }

bool VkApplication::checkValidationLayerSupport()
{
    uint32_t instanceLayerCount{0};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

    std::vector<VkLayerProperties> availableLayers(instanceLayerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableLayers.data()));

    std::vector<std::string> layerNames;
    std::transform(
        availableLayers.begin(), availableLayers.end(), std::back_inserter(layerNames),
        [](const VkLayerProperties &properties)
        { return properties.layerName; });

    log(Level::Info, "Found ", instanceLayerCount, " available Instance layer(s)");
    for (const auto &layer : layerNames)
    {
        log(Level::Info, layer);
    }

    for (const char *layerName : _validationLayers)
    {
        bool layerFound = false;
        for (const auto &layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }
    return true;
}

// // create shared _uploadCmd and begin
// void VkApplication::preHostDeviceIO()
// {
//     VkCommandBufferAllocateInfo allocInfo{};
//     allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//     allocInfo.commandPool = _commandPool;
//     allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//     allocInfo.commandBufferCount = 1;
//     VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &_uploadCmd));

//     VkCommandBufferBeginInfo cmdBufferBeginInfo{};
//     cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//     //  specifies that each recording of the command buffer will only be submitted once,
//     //  and the command buffer will be reset and recorded again between each submission.
//     cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//     VK_CHECK(vkBeginCommandBuffer(_uploadCmd, &cmdBufferBeginInfo));

//     VkFenceCreateInfo fenceInfo = {
//         .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0, // VK_FENCE_CREATE_SIGNALED_BIT: signaled state
//     };
//     VkFence fence;
//     VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_ioFence));
// }

// // end recording of buffer.
// // wait for completion using fence
// void VkApplication::postHostDeviceIO()
// {
//     VK_CHECK(vkEndCommandBuffer(_uploadCmd));

//     const VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;

//     VkSubmitInfo submitInfo{};
//     submitInfo.waitSemaphoreCount = 0;
//     submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
//     // only useful when having waitsemaphore
//     submitInfo.pWaitDstStageMask = &flags;
//     submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//     submitInfo.commandBufferCount = 1;
//     submitInfo.pCommandBuffers = &_uploadCmd;
//     submitInfo.signalSemaphoreCount = 0;
//     submitInfo.pSignalSemaphores = VK_NULL_HANDLE;
//     VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _ioFence));

//     const auto result = vkWaitForFences(_logicalDevice, 1, &_ioFence, VK_TRUE,
//                                         DEFAULT_FENCE_TIMEOUT);
//     if (result == VK_TIMEOUT)
//     {
//         vkDeviceWaitIdle(_logicalDevice);
//     }
//     // clean all the staging resources
//     vkDestroyBuffer(_logicalDevice, _stagingVb, nullptr);
//     vkDestroyBuffer(_logicalDevice, _stagingIb, nullptr);
//     // for texture
//     vkDestroyBuffer(_logicalDevice, _stagingImageBuffer, nullptr);
//     // for glb Scene
//     for (size_t i = 0; i < _stagingVbForMesh.size(); ++i)
//     {
//         vkDestroyBuffer(_logicalDevice, _stagingVbForMesh[i], nullptr);
//     }
//     for (size_t i = 0; i < _stagingIbForMesh.size(); ++i)
//     {
//         vkDestroyBuffer(_logicalDevice, _stagingIbForMesh[i], nullptr);
//     }
//     // for material buffer
//     vkDestroyBuffer(_logicalDevice, _stagingMatBuffer, nullptr);
//     vkDestroyBuffer(_logicalDevice, _stagingIndirectDrawBuffer, nullptr);
// }

// // cull face be careful
// // Interleaved vertex attributes
// void VkApplication::loadVao()
// {
//     std::vector<VertexDef1> vertices = {
//         {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
//         {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
//         {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
//         {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
//     };

//     std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

//     _indexCount = indices.size();

//     // vao
//     // staging buffer:
//     // 1. usage: VK_BUFFER_USAGE_TRANSFER_SRC_BIT
//     // 2. VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//     const auto vbByteSize = vertices.size() * sizeof(VertexDef1);
//     const auto ebByteSize = indices.size() * sizeof(uint32_t);
//     {
//         // create _stagingVb
//         VmaAllocation vmaStagingBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = vbByteSize,
//             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                      VMA_ALLOCATION_CREATE_MAPPED_BIT,
//             .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//             .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                                  &_stagingVb,
//                                  &vmaStagingBufferAllocation, nullptr));
//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingBufferAllocation, &mappedMemory));
//         memcpy(mappedMemory, vertices.data(), vbByteSize);
//         vmaUnmapMemory(_vmaAllocator, vmaStagingBufferAllocation);
//     }

//     {
//         // create stagingEb
//         VmaAllocation vmaStagingBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = ebByteSize,
//             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                      VMA_ALLOCATION_CREATE_MAPPED_BIT,
//             .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//             .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                                  &_stagingIb,
//                                  &vmaStagingBufferAllocation, nullptr));
//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingBufferAllocation, &mappedMemory));
//         memcpy(mappedMemory, indices.data(), ebByteSize);
//         vmaUnmapMemory(_vmaAllocator, vmaStagingBufferAllocation);
//     }

//     // device buffer:
//     // 1. usage: VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
//     // 2. VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
//     {
//         // create vbo
//         VmaAllocation vmaDeviceBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = vbByteSize,
//             .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo bufferAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
//                      VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
//             .usage = VMA_MEMORY_USAGE_GPU_ONLY,
//             .preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo,
//                                  &_deviceVb,
//                                  &vmaDeviceBufferAllocation, nullptr));

//         // src: bytesOffset, dst: bytesOffset
//         VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = vbByteSize};
//         vkCmdCopyBuffer(_uploadCmd, _stagingVb, _deviceVb, 1, &region);
//     }

//     {
//         // create ebo
//         VmaAllocation vmaDeviceBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = ebByteSize,
//             .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo bufferAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
//                      VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
//             .usage = VMA_MEMORY_USAGE_GPU_ONLY,
//             .preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo,
//                                  &_deviceIb,
//                                  &vmaDeviceBufferAllocation, nullptr));

//         // src: bytesOffset, dst: bytesOffset
//         VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = ebByteSize};
//         vkCmdCopyBuffer(_uploadCmd, _stagingIb, _deviceIb, 1, &region);
//     }
// }

// void VkApplication::loadTextures()
// {
//     // std::string filename = getAssetPath() + "metalplate01_rgba.ktx";
//     std::string filename = getAssetPath() + "lavaplanet_color_rgba.ktx";
//     VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
//     ktxResult result;
//     ktxTexture *ktxTexture;

// #if defined(__ANDROID__)
//     AAsset *asset = AAssetManager_open(_assetManager, filename.c_str(), AASSET_MODE_STREAMING);
//     if (!asset)
//     {
//         FATAL("Could not load texture from " + filename, -1);
//     }
//     size_t size = AAsset_getLength(asset);
//     ASSERT(size > 0, "asset size should larger then 0");

//     ktx_uint8_t *textureData = new ktx_uint8_t[size];
//     AAsset_read(asset, textureData, size);
//     AAsset_close(asset);
//     result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
//                                          &ktxTexture);
//     delete[] textureData;
// #else
//     // To Do
// #endif
//     ASSERT(result == KTX_SUCCESS, "ktxTexture_CreateFromMemory failed");
//     auto textureWidth = ktxTexture->baseWidth;
//     auto textureHeight = ktxTexture->baseHeight;
//     auto textureMipLevels = ktxTexture->numLevels;
//     ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
//     ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

//     // Linear tiled images
//     // Optimal tiled images: not accessible by the host, requires some sort of data copy,
//     // either from a buffer or	a linear tiled image
// #if defined(LINEAR_TILED_IMAGES)
//     VmaAllocation vmaImageAllocation{nullptr};

//     // linear texture
//     VkImageCreateInfo imageCreateInfo{};
//     imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//     imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//     imageCreateInfo.format = format;
//     imageCreateInfo.mipLevels = 1;
//     imageCreateInfo.arrayLayers = 1;
//     imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//     // linear tiling
//     imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
//     // if it is optim tiling, usage will be VK_IMAGE_USAGE_TRANSFER_SRC_BIT, a convert is needed
//     // other flag: VK_IMAGE_USAGE_TRANSFER_DST_BIT
//     // VK_IMAGE_USAGE_SAMPLED_BIT: directly used by shader
//     imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
//     imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//     // for optimal   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//     // VK_IMAGE_LAYOUT_PREINITIALIZED is only useful with linear images
//     imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
//     imageCreateInfo.extent = {textureWidth, textureHeight, 1};

//     // create a image, allocate memory for it, and bind them together, all in one call
//     // memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: use VMA_MEMORY_USAGE_AUTO_PREFER_HOST
//     // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/group__group__alloc.html#ggaa5846affa1e9da3800e3e78fae2305cca9b422585242160b8ed3418310ee6664d
//     // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT is needed for VMA_MEMORY_USAGE_AUTO_PREFER_HOST
//     // to map/unmap
//     const VmaAllocationCreateInfo allocCreateInfo = {
//         .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
//                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
//         .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
//         .priority = 1.0f,
//     };

//     VK_CHECK(vmaCreateImage(_vmaAllocator, &imageCreateInfo, &allocCreateInfo, &image,
//                             &vmaImageAllocation, nullptr));

//     if (vmaImageAllocation != nullptr)
//     {
//         VmaAllocationInfo imageAllocationInfo;
//         vmaGetAllocationInfo(_vmaAllocator, vmaImageAllocation, &imageAllocationInfo);

//         //        // Map image memory
//         //        void *data;
//         //        VK_CHECK_RESULT(vkMapMemory(device, mappableMemory, 0, memReqs.size, 0, &data));
//         //        // Copy image data of the first mip level into memory
//         //        memcpy(data, ktxTextureData, memReqs.size);
//         //        vkUnmapMemory(device, mappableMemory);

//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaImageAllocation, &mappedMemory));
//         memcpy(mappedMemory, ktxTextureData, imageAllocationInfo.size);
//         vmaUnmapMemory(_vmaAllocator, vmaImageAllocation);

//         // commandbuffer to submit the texture data
//         // image memory barrier transfer image to shader read layout
//         // transition image layout
//         VkCommandBuffer copyCmd;
//         VkCommandBufferAllocateInfo allocInfo{};
//         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//         allocInfo.commandPool = _commandPool;
//         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//         allocInfo.commandBufferCount = 1;
//         VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &copyCmd));
//         // recording buffer
//         VkCommandBufferBeginInfo cmdBufferBeginInfo{};
//         cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//         VK_CHECK(vkBeginCommandBuffer(copyCmd, &cmdBufferBeginInfo));

//         VkImageSubresourceRange subresourceRange = {};
//         subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         subresourceRange.baseMipLevel = 0;
//         subresourceRange.levelCount = 1;
//         subresourceRange.layerCount = 1;

//         VkImageMemoryBarrier imageMemoryBarrier{};
//         imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.image = image;
//         imageMemoryBarrier.subresourceRange = subresourceRange;
//         // from cpu write
//         imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
//         // to gpu read
//         imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//         // match creation
//         imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
//         imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         vkCmdPipelineBarrier(
//             copyCmd,
//             VK_PIPELINE_STAGE_HOST_BIT,
//             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//             0,
//             0, nullptr,
//             0, nullptr,
//             1, &imageMemoryBarrier);
//         VK_CHECK(vkEndCommandBuffer(copyCmd));

//         VkFenceCreateInfo fenceInfo = {
//             .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
//             .pNext = nullptr,
//             .flags = 0,
//         };
//         VkFence fence;
//         VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &fence));
//         VkSubmitInfo submitInfo{};
//         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//         submitInfo.commandBufferCount = 1;
//         submitInfo.pCommandBuffers = &copyCmd;
//         VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, fence));
//         // Wait for the fence to signal that command buffer has finished executing
//         VK_CHECK(vkWaitForFences(_logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
//         vkDestroyFence(_logicalDevice, fence, nullptr);
//         vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &copyCmd);
//     }
// #else
//     // This buffer is used as a transfer source for the buffer copy
//     VmaAllocation vmaStagingBufferAllocation{nullptr};
//     VkBufferCreateInfo bufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .size = size,
//         .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // for staging buffer
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//     };
//     const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//         .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                  VMA_ALLOCATION_CREATE_MAPPED_BIT,
//         .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//     };
//     VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                              &_stagingImageBuffer,
//                              &vmaStagingBufferAllocation, nullptr));
//     if (vmaStagingBufferAllocation != nullptr)
//     {
//         VmaAllocationInfo stagingBufferAllocationInfo;
//         vmaGetAllocationInfo(_vmaAllocator, vmaStagingBufferAllocation,
//                              &stagingBufferAllocationInfo);
//         // copy to staging buffer (visible both host and device)
//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingBufferAllocation, &mappedMemory));
//         memcpy(mappedMemory, ktxTextureData, ktxTextureSize);
//         vmaUnmapMemory(_vmaAllocator, vmaStagingBufferAllocation);
//         VK_CHECK(vmaFlushAllocation(_vmaAllocator, vmaStagingBufferAllocation, 0, ktxTextureSize));

//         // for image
//         // diff1: textureMipLevels,
//         // diff2: VK_IMAGE_TILING_OPTIMAL,
//         // diff3: usage has 1 more flag: VK_IMAGE_USAGE_TRANSFER_DST_BIT
//         // diff4: initial layout: VK_IMAGE_LAYOUT_UNDEFINED

//         VkImageCreateInfo imageCreateInfo{};
//         imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//         imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//         imageCreateInfo.format = format;
//         imageCreateInfo.mipLevels = textureMipLevels;
//         imageCreateInfo.arrayLayers = 1;
//         imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//         imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//         imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
//         imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//         imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//         imageCreateInfo.extent = {textureWidth, textureHeight, 1};
//         // no need for VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, cpu does not need access
//         const VmaAllocationCreateInfo allocCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
//             .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
//             .priority = 1.0f,
//         };
//         VK_CHECK(vmaCreateImage(_vmaAllocator, &imageCreateInfo, &allocCreateInfo, &_image,
//                                 &_vmaImageAllocation, nullptr));

//         std::vector<VkBufferImageCopy> bufferCopyRegions;
//         uint32_t offset = 0;

//         for (uint32_t i = 0; i < textureMipLevels; ++i)
//         {
//             ktx_size_t offsetForMipMapLevel;
//             auto ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offsetForMipMapLevel);
//             ASSERT(ret == KTX_SUCCESS, "ktxTexture_GetImageOffset failed");
//             // Setup a buffer image copy structure for the current mip level
//             VkBufferImageCopy bufferCopyRegion = {};
//             // regarding mipmap
//             bufferCopyRegion.bufferOffset = offsetForMipMapLevel;
//             // could be depth, stencil and color
//             bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//             bufferCopyRegion.imageSubresource.mipLevel = i;
//             bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
//             bufferCopyRegion.imageSubresource.layerCount = 1;
//             // primad mipmap hierachy
//             bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> i;
//             bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> i;
//             bufferCopyRegion.imageExtent.depth = 1;
//             bufferCopyRegions.push_back(bufferCopyRegion);
//         }

//         // now has mipmap
//         // barrier based on mip level, array layers
//         VkImageSubresourceRange subresourceRange = {};
//         subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         subresourceRange.baseMipLevel = 0;
//         subresourceRange.levelCount = textureMipLevels;
//         subresourceRange.layerCount = 1;

//         // transition layout
//         VkImageMemoryBarrier imageMemoryBarrier{};
//         imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.image = _image;
//         imageMemoryBarrier.subresourceRange = subresourceRange;
//         imageMemoryBarrier.srcAccessMask = 0;
//         imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//         imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//         // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: written into
//         imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

//         // it defines a memory dependency between commands that were
//         //  submitted to the same queue before it, and those submitted to the same queue after it.
//         //  ensure the image layout is VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, which could be write to.
//         vkCmdPipelineBarrier(
//             _uploadCmd,
//             VK_PIPELINE_STAGE_HOST_BIT,
//             VK_PIPELINE_STAGE_TRANSFER_BIT,
//             0,
//             0, nullptr,
//             0, nullptr,
//             1, &imageMemoryBarrier);
//         // now image layout(usage) is writable
//         // staging buffer to device-local(image is device local memory)
//         vkCmdCopyBufferToImage(
//             _uploadCmd,
//             _stagingImageBuffer,
//             _image,
//             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//             static_cast<uint32_t>(bufferCopyRegions.size()),
//             bufferCopyRegions.data());

//         // image layout(usage) from dst -> shader read
//         imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//         imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//         imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//         imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         vkCmdPipelineBarrier(
//             _uploadCmd,
//             VK_PIPELINE_STAGE_TRANSFER_BIT,
//             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//             0,
//             0, nullptr,
//             0, nullptr,
//             1, &imageMemoryBarrier);

//         //        // Done with the staging buffer
//         //        // vmaDestroyBuffer to replace the following
//         //        // Clean up staging resources
//         //        // vkFreeMemory(_logicalDevice, stagingBuffer, nullptr);
//         //        //vmaFreeMemory(_vmaAllocator, vmaImageAllocation);
//         //
//         //        vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
//         //        // cannot free memory bounded to the image.
//         //        // vmaDestroyBuffer(_vmaAllocator, stagingBuffer, vmaImageAllocation);
//     }
// #endif
//     // done with the cpu texture
//     ktxTexture_Destroy(ktxTexture);
//     // image view

//     // inteprete images's size, location and format except layout (image barrier)
//     VkImageViewCreateInfo imageViewInfo = {};
//     imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//     imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//     imageViewInfo.format = format;
//     // subresource range could limit miplevel and layer ranges, here all are open to access
//     imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     imageViewInfo.subresourceRange.baseMipLevel = 0;
//     imageViewInfo.subresourceRange.baseArrayLayer = 0;
//     imageViewInfo.subresourceRange.layerCount = 1;
// #if defined(LINEAR_TILED_IMAGES)
//     imageViewInfo.subresourceRange.levelCount = 1;
// #else
//     imageViewInfo.subresourceRange.levelCount = textureMipLevels;
// #endif
//     imageViewInfo.image = _image;
//     VK_CHECK(vkCreateImageView(_logicalDevice, &imageViewInfo, nullptr, &_imageView));

//     // sampler
//     VkSamplerCreateInfo samplerCreateInfo = {};
//     samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
//     samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
//     samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
//     samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
//     samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//     samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//     samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

//     samplerCreateInfo.mipLodBias = 0.0f;
//     samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
//     samplerCreateInfo.minLod = 0.0f;
// #if defined(LINEAR_TILED_IMAGES)
//     samplerCreateInfo.maxLod = 0.0f;
// #else
//     samplerCreateInfo.maxLod = textureMipLevels;
// #endif
//     // Enable anisotropic filtering
//     if (_enabledDeviceFeatures.features.samplerAnisotropy)
//     {
//         // Use max. level of anisotropy for this example
//         samplerCreateInfo.maxAnisotropy = _physicalDevicesProp1.limits.maxSamplerAnisotropy;
//         samplerCreateInfo.anisotropyEnable = VK_TRUE;
//     }
//     else
//     {
//         samplerCreateInfo.maxAnisotropy = 1.0;
//         samplerCreateInfo.anisotropyEnable = VK_FALSE;
//     }
//     samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
//     VK_CHECK(vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &_sampler));
// }

// // minimize round-trip between cpu-gpu
// void buildCompositeBuffer(const Scene &scene,
//                           std::vector<VkBuffer> &outBuffers,
//                           std::vector<VkSampler> &outSamplers)
// {
//     // Create vbo

//     //
//     //    buffers.emplace_back(context.createBuffer(
//     //            model.totalVertexSize,
//     // #if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
//     //            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
//     // #endif
//     //            VK_BUFFER_USAGE_TRANSFER_DST_BIT | (makeBuffersSuitableForAccelStruct
//     //                                                ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
//     //                                                : 0) |
//     //            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
//     //            VMA_MEMORY_USAGE_GPU_ONLY, "vertex"));
// }

// void VkApplication::loadGLB()
// {
//     std::string filename = getAssetPath() + "BoxTextured.glb";

//     // Load GLB
//     AAsset *glbAsset = AAssetManager_open(_assetManager, filename.c_str(), AASSET_MODE_BUFFER);
//     size_t glbByteSize = AAsset_getLength(glbAsset);
//     std::vector<char> glbContent;
//     glbContent.resize(glbByteSize);
//     AAsset_read(glbAsset, glbContent.data(), glbByteSize);

//     GltfBinaryIOReader reader;
//     std::shared_ptr<Scene> scene = reader.read(glbContent);
//     _numMeshes = scene->meshes.size();

//     // check device feature supported
//     if (_vk12features.bufferDeviceAddress)
//     {
//         {
//             // ssbo for vertices
//             auto bufferByteSize = scene->totalVerticesByteSize;
//             _compositeVBSizeInByte = bufferByteSize;
//             VkBufferUsageFlags bufferUsageFlag{
//                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
//                 //                    | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
//                 | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
//             VmaMemoryUsage memoryUsage{
//                 VMA_MEMORY_USAGE_GPU_ONLY};
//             VmaAllocation vmaCompositeVerticeBufferAllocation{nullptr};
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = bufferByteSize,
//                 .usage = bufferUsageFlag,
//             };
//             // for device buffer
//             // VK_MEMORY_PROPERTY_HOST_CACHED_BIT bit specifies that memory allocated with this type is cached on the host
//             const VmaAllocationCreateInfo deviceBufferAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = memoryUsage,
//                 .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT};
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo,
//                                      &deviceBufferAllocationCreateInfo,
//                                      &_compositeVB,
//                                      &vmaCompositeVerticeBufferAllocation, nullptr));
//         }

//         {
//             // ssbo for ib
//             auto bufferByteSize = scene->totalIndexByteSize;
//             _compositeIBSizeInByte = bufferByteSize;
//             VkBufferUsageFlags bufferUsageFlag{
//                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};
//             VmaMemoryUsage memoryUsage{
//                 VMA_MEMORY_USAGE_GPU_ONLY};

//             VmaAllocation vmaCompositeIndicesBufferAllocation{VK_NULL_HANDLE};
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = bufferByteSize,
//                 .usage = bufferUsageFlag,
//             };

//             // for device buffer
//             // VK_MEMORY_PROPERTY_HOST_CACHED_BIT bit specifies that memory allocated with this type is cached on the host
//             const VmaAllocationCreateInfo deviceBufferAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = memoryUsage,
//                 .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT};

//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo,
//                                      &deviceBufferAllocationCreateInfo,
//                                      &_compositeIB,
//                                      &vmaCompositeIndicesBufferAllocation, nullptr));
//         }

//         // upload data to buffer
//         uint32_t currentVertexStartingIndex = 0u;
//         uint32_t currentIndicesStartingIndex = 0u;
//         // firstIndex of the composite index buffer for current draw
//         uint32_t firstIndex = 0u;
//         // offset into composite vertice buffer
//         uint32_t vertexOffset = 0u;
//         std::vector<IndirectDrawForVulkan> indirectDrawParams;
//         indirectDrawParams.reserve(scene->meshes.size());
//         uint32_t deviceCompositeVertexBufferOffsetInBytes = 0u;
//         uint32_t deviceCompositeIndicesBufferOffsetInBytes = 0u;
//         size_t meshId = 0;
//         for (const auto &mesh : scene->meshes)
//         {
//             auto vertexByteSizeMesh = sizeof(Vertex) * mesh.vertices.size();
//             auto vertexBufferPtr = reinterpret_cast<const void *>(mesh.vertices.data());
//             // to upload data from host to device, needs stagingBuffer
//             // create stagingVbForMesh to streaming only this mesh to device buffer
//             VmaAllocation vmaStagingMeshVerticesBufferAllocation{nullptr};
//             VkBufferCreateInfo verticeBufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = vertexByteSizeMesh,
//                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//             };
//             const VmaAllocationCreateInfo stagingVerticeBufferAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//                 .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//             VkBuffer stagingVerticeBuffer;
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &verticeBufferCreateInfo,
//                                      &stagingVerticeBufferAllocationCreateInfo,
//                                      &stagingVerticeBuffer,
//                                      &vmaStagingMeshVerticesBufferAllocation, nullptr));
//             _stagingVbForMesh.push_back(stagingVerticeBuffer);
//             // copy vb from host to device, region
//             void *mappedMemory{nullptr};
//             VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingMeshVerticesBufferAllocation,
//                                   &mappedMemory));
//             memcpy(mappedMemory, vertexBufferPtr, vertexByteSizeMesh);
//             vmaUnmapMemory(_vmaAllocator, vmaStagingMeshVerticesBufferAllocation);
//             // cmd to copy from staging to device
//             VkBufferCopy region{.srcOffset = 0,
//                                 .dstOffset = deviceCompositeVertexBufferOffsetInBytes,
//                                 .size = vertexByteSizeMesh};
//             vkCmdCopyBuffer(_uploadCmd, stagingVerticeBuffer, _compositeVB, 1, &region);

//             deviceCompositeVertexBufferOffsetInBytes += vertexByteSizeMesh;

//             // copy ib from host to device
//             auto indicesByteSizeMesh = sizeof(uint32_t) * mesh.indices.size();
//             auto indicesBufferPtr = reinterpret_cast<const void *>(mesh.indices.data());

//             VmaAllocation vmaStagingMeshIndiceBufferAllocation{nullptr};
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = indicesByteSizeMesh,
//                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//             };
//             const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//                 .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//             VkBuffer stagingIndiceBuffer;
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                                      &stagingIndiceBuffer,
//                                      &vmaStagingMeshIndiceBufferAllocation, nullptr));
//             _stagingIbForMesh.push_back(stagingIndiceBuffer);
//             // copy ib from host to device, region
//             void *mappedMemoryForIB{nullptr};
//             VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingMeshIndiceBufferAllocation,
//                                   &mappedMemoryForIB));
//             memcpy(mappedMemoryForIB, indicesBufferPtr, indicesByteSizeMesh);
//             vmaUnmapMemory(_vmaAllocator, vmaStagingMeshIndiceBufferAllocation);

//             // cmd to copy from staging to device
//             VkBufferCopy regionForIB{.srcOffset = 0,
//                                      .dstOffset = deviceCompositeIndicesBufferOffsetInBytes,
//                                      .size = indicesByteSizeMesh};
//             vkCmdCopyBuffer(_uploadCmd, stagingIndiceBuffer, _compositeIB, 1, &regionForIB);

//             deviceCompositeIndicesBufferOffsetInBytes += indicesByteSizeMesh;
//             // reserve still needs push_back/emplace_back
//             indirectDrawParams.emplace_back(IndirectDrawForVulkan{
//                 .indexCount = uint32_t(mesh.indices.size()),
//                 .instanceCount = 1,
//                 .firstIndex = firstIndex,
//                 .vertexOffset = static_cast<int>(vertexOffset),
//                 .firstInstance = 0,
//                 .meshId = static_cast<uint32_t>(meshId),
//                 .materialIndex = static_cast<uint32_t>(mesh.materialIdx),
//             });
//             vertexOffset += mesh.vertices.size();
//             firstIndex += mesh.indices.size();
//             ++meshId;
//         }
//         // textures
//         // 1. create image
//         // 2. create image view
//         // 3. upload through stage buffer
//         for (const auto &texture : scene->textures)
//         {
//             const auto textureMipLevels = getMipLevelsCount(texture->width,
//                                                             texture->height);
//             const auto format{VK_FORMAT_R8G8B8A8_UNORM};
//             VkImageCreateInfo imageCreateInfo{};
//             imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//             imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//             imageCreateInfo.format = format;
//             imageCreateInfo.mipLevels = textureMipLevels;
//             imageCreateInfo.arrayLayers = 1;
//             imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//             imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//             // usage here: both dst and src as mipmap generation
//             imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//             imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//             imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//             imageCreateInfo.extent = {static_cast<uint32_t>(texture->width),
//                                       static_cast<uint32_t>(texture->height), 1};
//             // no need for VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, cpu does not need access
//             const VmaAllocationCreateInfo allocCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
//                 .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
//                 .priority = 1.0f,
//             };
//             VkImage glbImage;
//             VmaAllocation glbImageAllocation;
//             VkImageView glbImageView;
//             VK_CHECK(vmaCreateImage(_vmaAllocator, &imageCreateInfo, &allocCreateInfo, &glbImage,
//                                     &glbImageAllocation, nullptr));
//             // image view
//             VkImageViewCreateInfo imageViewInfo = {};
//             imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//             imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//             imageViewInfo.format = format;
//             // subresource range could limit miplevel and layer ranges, here all are open to access
//             imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//             imageViewInfo.subresourceRange.baseMipLevel = 0;
//             imageViewInfo.subresourceRange.baseArrayLayer = 0;
//             imageViewInfo.subresourceRange.layerCount = 1;
// #if defined(LINEAR_TILED_IMAGES)
//             imageViewInfo.subresourceRange.levelCount = 1;
// #else
//             imageViewInfo.subresourceRange.levelCount = textureMipLevels;
// #endif
//             imageViewInfo.image = glbImage;
//             VK_CHECK(vkCreateImageView(_logicalDevice, &imageViewInfo, nullptr, &glbImageView));
//             this->_glbImages.emplace_back(glbImage);
//             this->_glbImageAllocation.emplace_back(glbImageAllocation);
//             this->_glbImageViews.emplace_back(glbImageView);

//             // staging buffer
//             ASSERT(glbImageAllocation, "glbImageAllocation should be defined");

//             VmaAllocationInfo glbImageAllocationInfo;
//             vmaGetAllocationInfo(_vmaAllocator, glbImageAllocation, &glbImageAllocationInfo);
//             const auto stagingBufferSize = glbImageAllocationInfo.size;

//             VmaAllocation vmaStagingBufferAllocation{nullptr};
//             VkBuffer glbImageStagingBuffer;
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = stagingBufferSize,
//                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // for staging buffer
//                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//             };
//             const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//             };
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                                      &glbImageStagingBuffer,
//                                      &vmaStagingBufferAllocation, nullptr));
//             _glbImageStagingBuffer.emplace_back(glbImageStagingBuffer);
//             if (vmaStagingBufferAllocation != nullptr)
//             {
//                 void *imageDataPtr{nullptr};
//                 // format: VK_FORMAT_R8G8B8A8_UNORM took 4 bytes
//                 const auto imageDataSizeInBytes = texture->width * texture->height * 1 * 4;
//                 VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingBufferAllocation,
//                                       &imageDataPtr));
//                 memcpy(imageDataPtr, texture->data, imageDataSizeInBytes);
//                 vmaUnmapMemory(_vmaAllocator, vmaStagingBufferAllocation);
//                 // image layout from undefined to write dst
//                 // transition layout
//                 // barrier based on mip level, array layers
//                 VkImageSubresourceRange subresourceRange = {};
//                 subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//                 subresourceRange.baseMipLevel = 0;
//                 subresourceRange.levelCount = textureMipLevels;
//                 subresourceRange.layerCount = 1;

//                 VkImageMemoryBarrier imageMemoryBarrier{};
//                 imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//                 imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//                 imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//                 imageMemoryBarrier.image = glbImage;
//                 imageMemoryBarrier.subresourceRange = subresourceRange;
//                 imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE; // 0: VK_ACCESS_NONE
//                 imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//                 imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//                 // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: written into
//                 imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

//                 vkCmdPipelineBarrier(
//                     _uploadCmd,
//                     VK_PIPELINE_STAGE_HOST_BIT,
//                     VK_PIPELINE_STAGE_TRANSFER_BIT,
//                     0,
//                     0, nullptr,
//                     0, nullptr,
//                     1, &imageMemoryBarrier);
//                 // now image layout(usage) is writable
//                 // staging buffer to device-local(image is device local memory)
//                 VkBufferImageCopy bufferCopyRegion = {};
//                 // mipmap level0: original copy
//                 bufferCopyRegion.bufferOffset = 0;
//                 // could be depth, stencil and color
//                 bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//                 bufferCopyRegion.imageSubresource.mipLevel = 0;
//                 bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
//                 bufferCopyRegion.imageSubresource.layerCount = 1;
//                 bufferCopyRegion.imageOffset.x = bufferCopyRegion.imageOffset.y =
//                     bufferCopyRegion.imageOffset.z = 0;
//                 // primad mipmap hierachy
//                 bufferCopyRegion.imageExtent.width = texture->width;
//                 bufferCopyRegion.imageExtent.height = texture->height;
//                 bufferCopyRegion.imageExtent.depth = 1;
//                 vkCmdCopyBufferToImage(
//                     _uploadCmd,
//                     glbImageStagingBuffer,
//                     glbImage,
//                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//                     1,
//                     &bufferCopyRegion);

//                 {
//                     // generate mipmaps
//                     // sample: texturemipmapgen
//                     VkFormatProperties formatProperties;
//                     vkGetPhysicalDeviceFormatProperties(_selectedPhysicalDevice,
//                                                         format, &formatProperties);
//                     ASSERT(formatProperties.optimalTilingFeatures &
//                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
//                            "Selected Physical Device cannot generate mipmaps");

//                     VkImageSubresourceRange subresourceRange = {};
//                     subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//                     subresourceRange.baseMipLevel = 0;
//                     subresourceRange.levelCount = 1;
//                     subresourceRange.baseArrayLayer = 0;
//                     subresourceRange.layerCount = 1;

//                     VkImageMemoryBarrier imageMemoryBarrier{};
//                     imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//                     imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//                     imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//                     imageMemoryBarrier.image = glbImage;
//                     imageMemoryBarrier.subresourceRange = subresourceRange;
//                     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//                     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//                     imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//                     imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

//                     int32_t w = texture->width;
//                     int32_t h = texture->height;
//                     for (uint32_t i = 1; i <= textureMipLevels; ++i)
//                     {
//                         // Prepare current mip level as image blit source for next level
//                         imageMemoryBarrier.subresourceRange.baseMipLevel = i - 1;
//                         vkCmdPipelineBarrier(
//                             _uploadCmd,
//                             VK_PIPELINE_STAGE_TRANSFER_BIT,
//                             VK_PIPELINE_STAGE_TRANSFER_BIT,
//                             0,
//                             0, nullptr,
//                             0, nullptr,
//                             1, &imageMemoryBarrier);
//                         // level0 write, barrier, level0 read, level 1write, barrier
//                         // level1 read, ....
//                         if (i == textureMipLevels)
//                         {
//                             break;
//                         }
//                         const int32_t newW = w > 1 ? w >> 1 : w;
//                         const int32_t newH = h > 1 ? h >> 1 : h;

//                         VkImageBlit imageBlit{};
//                         imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//                         imageBlit.srcSubresource.layerCount = 1;
//                         imageBlit.srcSubresource.baseArrayLayer = 0;
//                         imageBlit.srcSubresource.mipLevel = i - 1;
//                         imageBlit.srcOffsets[0].x = imageBlit.srcOffsets[0].y = imageBlit.srcOffsets[0].z = 0;
//                         imageBlit.srcOffsets[1].x = w;
//                         imageBlit.srcOffsets[1].y = h;
//                         imageBlit.srcOffsets[1].z = 1;

//                         imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//                         imageBlit.dstSubresource.layerCount = 1;
//                         imageBlit.srcSubresource.baseArrayLayer = 0;
//                         imageBlit.dstSubresource.mipLevel = i;
//                         imageBlit.dstOffsets[1].x = newW;
//                         imageBlit.dstOffsets[1].y = newH;
//                         imageBlit.dstOffsets[1].z = 1;

//                         vkCmdBlitImage(_uploadCmd,
//                                        glbImage,
//                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//                                        glbImage,
//                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//                                        1,
//                                        &imageBlit,
//                                        VK_FILTER_LINEAR);
//                         w = newW;
//                         h = newH;
//                     }
//                     // all mip layers are in TRANSFER_SRC --> SHADER_READ
//                     const VkImageMemoryBarrier convertToShaderReadBarrier = {
//                         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//                         .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
//                         .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
//                         .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//                         .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//                         .image = glbImage,
//                         .subresourceRange =
//                             {
//                                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//                                 .baseMipLevel = 0,
//                                 .levelCount = textureMipLevels,
//                                 .baseArrayLayer = 0,
//                                 .layerCount = 1,
//                             },

//                     };
//                     vkCmdPipelineBarrier(_uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
//                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
//                                          nullptr,
//                                          1, &convertToShaderReadBarrier);
//                 }
//             }
//         }
//         // sampler
//         {
//             VkSampler sampler;
//             VkSamplerCreateInfo samplerCreateInfo = {};
//             samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
//             samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
//             samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
//             samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
//             samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//             samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//             samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

//             samplerCreateInfo.mipLodBias = 0.0f;
//             samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
//             samplerCreateInfo.minLod = 0.0f;
// #if defined(LINEAR_TILED_IMAGES)
//             samplerCreateInfo.maxLod = 0.0f;
// #else
//             samplerCreateInfo.maxLod = 10.f;
// #endif
//             // Enable anisotropic filtering
//             if (_enabledDeviceFeatures.features.samplerAnisotropy)
//             {
//                 // Use max. level of anisotropy for this example
//                 samplerCreateInfo.maxAnisotropy = _physicalDevicesProp1.limits.maxSamplerAnisotropy;
//                 samplerCreateInfo.anisotropyEnable = VK_TRUE;
//             }
//             else
//             {
//                 samplerCreateInfo.maxAnisotropy = 1.0;
//                 samplerCreateInfo.anisotropyEnable = VK_FALSE;
//             }
//             samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
//             VK_CHECK(vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &sampler));
//             _glbSamplers.emplace_back(sampler);
//         }

//         // packing materials into composite buffer
//         const auto materialByteSize = sizeof(Material) * scene->materials.size();
//         {
//             // create device buffer
//             auto bufferByteSize = materialByteSize;
//             _compositeMatBSizeInByte = bufferByteSize;
//             VkBufferUsageFlags bufferUsageFlag{
//                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
//             VmaMemoryUsage memoryUsage{
//                 VMA_MEMORY_USAGE_GPU_ONLY};

//             VmaAllocation vmaMaterialBufferAllocation{VK_NULL_HANDLE};
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = bufferByteSize,
//                 .usage = bufferUsageFlag,
//             };

//             // for device buffer
//             // VK_MEMORY_PROPERTY_HOST_CACHED_BIT bit specifies that memory allocated with this type is cached on the host
//             const VmaAllocationCreateInfo deviceBufferAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = memoryUsage,
//                 .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT};
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo,
//                                      &deviceBufferAllocationCreateInfo,
//                                      &_compositeMatB,
//                                      &vmaMaterialBufferAllocation, nullptr));
//         }
//         {
//             // create staging buffer
//             auto materialBufferPtr = reinterpret_cast<const void *>(scene->materials.data());
//             // staging buffer for matBuffer
//             VmaAllocation vmaStagingMatBufferAllocation{nullptr};
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = materialByteSize,
//                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//             };
//             const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//                 .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo,
//                                      &stagingAllocationCreateInfo,
//                                      &_stagingMatBuffer,
//                                      &vmaStagingMatBufferAllocation, nullptr));
//             // copy matBuffer from host to device, region
//             void *mappedMemoryForMatB{nullptr};
//             VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingMatBufferAllocation,
//                                   &mappedMemoryForMatB));
//             memcpy(mappedMemoryForMatB, materialBufferPtr, materialByteSize);
//             vmaUnmapMemory(_vmaAllocator, vmaStagingMatBufferAllocation);
//         }
//         {
//             // cmd to copy from staging to device
//             VkBufferCopy regionForMatB{.srcOffset = 0,
//                                        .dstOffset = 0,
//                                        .size = materialByteSize};
//             vkCmdCopyBuffer(_uploadCmd, _stagingMatBuffer, _compositeMatB, 1, &regionForMatB);
//         }

//         // packing for indirectDrawBuffer
//         const auto indirectDrawBufferByteSize =
//             sizeof(IndirectDrawForVulkan) * indirectDrawParams.size();
//         {
//             // create device buffer for indirectDraw
//             auto bufferByteSize = indirectDrawBufferByteSize;
//             _indirectDrawBSizeInByte = bufferByteSize;
//             // both ib and indirectDraw buffer have flag: VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
//             VkBufferUsageFlags bufferUsageFlag{
//                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};
//             VmaMemoryUsage memoryUsage{
//                 VMA_MEMORY_USAGE_GPU_ONLY};

//             VmaAllocation vmaIndirectDrawBufferAllocation{VK_NULL_HANDLE};
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = bufferByteSize,
//                 .usage = bufferUsageFlag,
//             };

//             // for device buffer
//             // VK_MEMORY_PROPERTY_HOST_CACHED_BIT bit specifies that memory allocated with this type is cached on the host
//             const VmaAllocationCreateInfo deviceBufferAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = memoryUsage,
//                 .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT};
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo,
//                                      &deviceBufferAllocationCreateInfo,
//                                      &_indirectDrawB,
//                                      &vmaIndirectDrawBufferAllocation, nullptr));
//         }
//         {
//             // create staging buffer
//             auto indirectDrawBufferPtr = reinterpret_cast<const void *>(indirectDrawParams.data());
//             // staging buffer for indirectDrawBuffer
//             VmaAllocation vmaStagingIndirectDrawBufferAllocation{nullptr};
//             VkBufferCreateInfo bufferCreateInfo{
//                 .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//                 .size = indirectDrawBufferByteSize,
//                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//             };
//             const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//                 .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
//                 .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//                 .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//             VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo,
//                                      &stagingAllocationCreateInfo,
//                                      &_stagingIndirectDrawBuffer,
//                                      &vmaStagingIndirectDrawBufferAllocation, nullptr));
//             // copy IndirectDrawBuffer from host to device, region
//             void *mappedMemoryForIndirectDrawBuffer{nullptr};
//             VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingIndirectDrawBufferAllocation,
//                                   &mappedMemoryForIndirectDrawBuffer));
//             memcpy(mappedMemoryForIndirectDrawBuffer, indirectDrawBufferPtr,
//                    indirectDrawBufferByteSize);
//             vmaUnmapMemory(_vmaAllocator, vmaStagingIndirectDrawBufferAllocation);
//         }
//         {
//             // cmd to copy from staging to device
//             VkBufferCopy region{.srcOffset = 0,
//                                 .dstOffset = 0,
//                                 .size = indirectDrawBufferByteSize};
//             vkCmdCopyBuffer(_uploadCmd, _stagingIndirectDrawBuffer, _indirectDrawB, 1, &region);
//         }
//     }
// }