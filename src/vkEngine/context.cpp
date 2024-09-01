#include <vector>
#include <array>
#include <context.h>
#include <window.h>

#define VK_NO_PROTOTYPES // for volk
#define VOLK_IMPLEMENTATION

// triple-buffer
static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
static constexpr int MAX_DESCRIPTOR_SETS = 1 * MAX_FRAMES_IN_FLIGHT + 1 + 4;
// static constexpr int MAX_DESCRIPTOR_SETS = 1000;
//  Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void * /* pUserData */)
{
    // LOGE("MessageID: %s %d \nMessage: %s\n\n", pCallbackData->pMessageIdName,
    //      pCallbackData->messageIdNumber, pCallbackData->pMessage);
    return VK_FALSE;
}

class VkContext::Impl
{
public:
    explicit Impl(
        const VkContext &ctx,
        const Window &window,
        const std::vector<const char *> &instanceValidationLayers,
        const std::set<std::string> &instanceExtensions,
        const std::vector<const char *> deviceExtensions)
        : _window(window),
          _instanceValidationLayers(instanceValidationLayers),
          _instanceExtensions(instanceExtensions),
          _deviceExtensions(deviceExtensions)
    {
        createInstance();
        createSurface();
        selectPhysicalDevice();
        cacheSupportedSurfaceFormats();
        queryPhysicalDeviceCaps();
        selectQueueFamily();
        selectFeatures();
        createLogicDevice();
        cacheCommandQueue();
        createVMA();
    }

    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl(Impl &&) noexcept = default;
    Impl &operator=(Impl &&) noexcept = default;

    ~Impl()
    {
        vkDeviceWaitIdle(_logicalDevice);

        for (size_t i = 0; i < _swapChainImageViews.size(); i++)
        {
            vkDestroyImageView(_logicalDevice, _swapChainImageViews[i], nullptr);
        }
        // image is owned by swap chain
        vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);

        vmaDestroyAllocator(_vmaAllocator);
        vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_logicalDevice, nullptr);
        vkDestroyInstance(_instance, nullptr);
    }

    void createSwapChain();

    void createSwapChainImageView();

    VkRenderPass createSwapChainRenderPass();

    VkFramebuffer createFramebuffer(
        const std::string &name,
        VkRenderPass renderPass,
        const std::vector<VkImageView> &colors,
        const VkImageView depth,
        const VkImageView stencil,
        uint32_t width,
        uint32_t height);

    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> createGraphicsCommandBuffers(
        const std::string &name,
        uint32_t count,
        uint32_t inflightCount,
        VkFenceCreateFlags flags);

    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> createTransferCommandBuffers(
        const std::string &name,
        uint32_t count,
        uint32_t inflightCount,
        VkFenceCreateFlags flags);

    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createPersistentBuffer(
        const std::string &name,
        VkDeviceSize bufferSizeInBytes,
        VkBufferUsageFlags bufferUsageFlag);

    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createStagingBuffer(
        const std::string &name,
        VkDeviceSize bufferSizeInBytes);

    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createDeviceLocalBuffer(
        const std::string &name,
        VkDeviceSize bufferSizeInBytes,
        VkBufferUsageFlags bufferUsageFlag);

    std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> createImage(
        const std::string &name,
        VkImageType imageType,
        VkFormat format,
        VkExtent3D extent,
        uint32_t textureMipLevelCount,
        uint32_t textureLayersCount,
        VkSampleCountFlagBits textureMultiSampleCount,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memoryFlags,
        bool generateMips);

    std::tuple<VkSampler> createSampler(const std::string &name);

    std::vector<VkDescriptorSetLayout> createDescriptorSetLayout(std::vector<std::vector<VkDescriptorSetLayoutBinding>> &setBindings);

    void writeBuffer(
        const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
        const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &deviceLocalBuffer,
        const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer,
        const void *rawData,
        uint32_t sizeInBytes,
        uint32_t srcOffset = 0,
        uint32_t dstOffset = 0);

    void writeImage(
        const std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> &image,
        const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
        const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer,
        void *rawData);

    void generateMipmaps(
        const std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> &image,
        const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
        const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer);

    inline auto getLogicDevice() const
    {
        return _logicalDevice;
    }

    inline auto getVmaAllocator() const
    {
        return _vmaAllocator;
    }

    inline auto getInstance() const
    {
        return _instance;
    }

    inline auto getGraphicsQueue() const
    {
        return _graphicsQueue;
    }

    inline auto getPresentationQueue() const
    {
        return _presentationQueue;
    }

    // to be refactored into Physical Device entity
    inline auto getSelectedPhysicalDevice() const
    {
        return _selectedPhysicalDevice;
    }

    inline auto getSelectedPhysicalDeviceProp() const
    {
        return _physicalDevicesProp1;
    }

    inline auto getSurfaceKHR() const
    {
        return _surface;
    }

    inline auto getGraphicsComputeQueueFamilyIndex() const
    {
        return _graphicsComputeQueueFamilyIndex;
    }

    inline auto getPresentQueueFamilyIndex() const
    {
        return _presentQueueFamilyIndex;
    }

    inline auto getVk12FeatureCaps() const
    {
        return _vk12features;
    }

    inline auto getSwapChain() const
    {
        return _swapChain;
    }

    inline auto getSwapChainExtent() const
    {
        return _swapChainExtent;
    }

    inline const auto &getSwapChainImageViews() const
    {
        return _swapChainImageViews;
    }

private:
    void createInstance()
    {
        log(Level::Info, "createInstance");
        assert(checkValidationLayerSupport());
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

        const auto &instanceExtensionSets = _instanceExtensions;
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
                static_cast<uint32_t>(_instanceValidationLayers.size());
            createInfo.ppEnabledLayerNames = _instanceValidationLayers.data();
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

    void createSurface()
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

    void selectPhysicalDevice()
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

    void queryPhysicalDeviceCaps()
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

    void cacheSupportedSurfaceFormats()
    {
        uint32_t formatCount{0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(_selectedPhysicalDevice, _surface, &formatCount, nullptr);
        _surfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(_selectedPhysicalDevice, _surface, &formatCount,
                                             _surfaceFormats.data());
    }

    void selectQueueFamily()
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

    void selectFeatures();

    void createLogicDevice();

    void cacheCommandQueue();

    void createVMA();

    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> createCommandBuffers(
        const std::string &name,
        uint32_t count,
        uint32_t inflightCount,
        VkFenceCreateFlags flags,
        uint32_t queueFamilyIndex,
        VkQueue queue);

    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createBuffer(
        const std::string &name,
        VkDeviceSize bufferSizeInBytes,
        VkBufferUsageFlags bufferUsageFlag,
        VkSharingMode bufferSharingMode,
        VmaAllocationCreateFlags memoryAllocationFlag,
        VkMemoryPropertyFlags requiredMemoryProperties,
        VkMemoryPropertyFlags preferredMemoryProperties,
        VmaMemoryUsage memoryUsage);

    bool checkValidationLayerSupport() const
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

        for (const char *layerName : _instanceValidationLayers)
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

    // vkGetPhysicalDeviceFeatures2 + linkedlist to fill in
    inline bool checkRayTracingSupport()
    {
        return (_accelStructFeature.accelerationStructure &&
                _rayTracingFeature.rayTracingPipeline && _rayQueryFeature.rayQuery);
    }

    inline bool isMultiviewSupported() const
    {
        return _vk11features.multiview;
    }

    inline bool isFragmentDensityMapSupported() const
    {
        return _fragmentDensityMapFeature.fragmentDensityMap == VK_TRUE;
    }
    const Window &_window;
    const std::vector<const char *> _instanceValidationLayers;
    const std::set<std::string> &_instanceExtensions;
    const std::vector<const char *> _deviceExtensions;

    bool _enableValidationLayers{true};

    VkInstance _instance{VK_NULL_HANDLE};
    VkSurfaceKHR _surface{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT _debugMessenger;

    VkPhysicalDevice _selectedPhysicalDevice{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties _physicalDevicesProp1;
    // physical device features
    bool _bindlessSupported{false};
    bool _protectedMemory{false};

    uint32_t _graphicsComputeQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t _computeQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t _transferQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};
    // family queue of discrete gpu support the surface of native window
    uint32_t _presentQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};

    // each queue family have many queues (16)
    // I choose 0 as graphics, 1 as compute
    uint32_t _graphicsQueueIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t _computeQueueIndex{std::numeric_limits<uint32_t>::max()};

    VkStructChain<> _featureChain;

    // features supported by the selected physical device
    // KHR, ext, not in core features
    // VkPhysicalDeviceAccelerationStructureFeaturesKHR
    // VkPhysicalDeviceRayQueryFeaturesKHR
    // nv specific
    // VkPhysicalDeviceMeshShaderFeaturesNV
    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM _fragmentDensityMapOffsetFeature{
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
        .pNext = nullptr,
    };

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT _fragmentDensityMapFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
        .pNext = &_fragmentDensityMapOffsetFeature,
    };

    VkPhysicalDeviceMeshShaderFeaturesNV _meshShaderFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV,
        .pNext = (void *)&_fragmentDensityMapFeature,
    };

    VkPhysicalDeviceRayQueryFeaturesKHR _rayQueryFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
        .pNext = (void *)&_meshShaderFeature,
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR _rayTracingFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = (void *)&_rayQueryFeature,
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR _accelStructFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = (void *)&_rayTracingFeature,
    };

    VkPhysicalDeviceVulkan11Features _vk11features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = (void *)&_accelStructFeature,
    };
    VkPhysicalDeviceVulkan12Features _vk12features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = (void *)&_vk11features,
    };
    VkPhysicalDeviceVulkan13Features _vk13features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = (void *)&_vk12features,
    };
    VkPhysicalDeviceFeatures2 _physicalFeatures2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = (void *)&_vk13features,
    };
    // included in v1.1
    // VkPhysicalDeviceMultiviewFeatures
    // included in v1.2
    // VkPhysicalDeviceBufferDeviceAddressFeatures
    // VkPhysicalDeviceDescriptorIndexingFeatures
    // VkPhysicalDeviceTimelineSemaphoreFeatures

    // device properties
    VkPhysicalDeviceProperties2 _properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        //.pNext = &rayTracingPipelineProperties_,
    };
    // header of linked list to create logic device
    VkPhysicalDeviceFeatures2 _enabledDeviceFeatures;

    VkDevice _logicalDevice{VK_NULL_HANDLE};

    VkQueue _graphicsQueue{VK_NULL_HANDLE};
    VkQueue _computeQueue{VK_NULL_HANDLE};
    VkQueue _transferQueue{VK_NULL_HANDLE};
    VkQueue _presentationQueue{VK_NULL_HANDLE};
    VkQueue _sparseQueues{VK_NULL_HANDLE};

    VmaAllocator _vmaAllocator{VK_NULL_HANDLE};

    std::vector<VkSurfaceFormatKHR> _surfaceFormats;
    VkFormat _swapChainFormat{VK_FORMAT_B8G8R8A8_UNORM};
    VkColorSpaceKHR _colorspace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkExtent2D _swapChainExtent;
    VkSwapchainKHR _swapChain{VK_NULL_HANDLE};
    std::vector<VkImageView> _swapChainImageViews;
    // fbo for swapchain
    std::vector<VkFramebuffer> _swapChainFramebuffers;
};

void VkContext::Impl::selectFeatures()
{
    // query all features through single linked list
    // physicalFeatures2 --> indexing_features --> dynamicRenderingFeatures --> nullptr;
    // this gets the capabilities
    vkGetPhysicalDeviceFeatures2(_selectedPhysicalDevice, &_physicalFeatures2);

    // enable features
    sPhysicalDeviceFeatures2.features.independentBlend = VK_TRUE;
    sPhysicalDeviceFeatures2.features.vertexPipelineStoresAndAtomics = VK_TRUE;
    sPhysicalDeviceFeatures2.features.fragmentStoresAndAtomics = VK_TRUE;
    sPhysicalDeviceFeatures2.features.multiDrawIndirect = VK_TRUE;
    sPhysicalDeviceFeatures2.features.drawIndirectFirstInstance = VK_TRUE;
    sPhysicalDeviceFeatures2.features.independentBlend = VK_TRUE;
    //  vkCreateSampler(): pCreateInfo->anisotropyEnable
    // wrong
    // sPhysicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
    sPhysicalDeviceFeatures2.features.samplerAnisotropy = VK_TRUE;

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

void VkContext::Impl::createLogicDevice()
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
        static_cast<uint32_t>(_instanceValidationLayers.size());
    logicDeviceCreateInfo.ppEnabledLayerNames = _instanceValidationLayers.data();
    // when using VkPhysicalDeviceFeatures2, pEnabledFeatures set to be nullptr;
    logicDeviceCreateInfo.pNext = _featureChain.header();
    logicDeviceCreateInfo.pEnabledFeatures = nullptr;

    VK_CHECK(
        vkCreateDevice(_selectedPhysicalDevice, &logicDeviceCreateInfo, nullptr,
                       &_logicalDevice));
    ASSERT(_logicalDevice, "Failed to create logic device");

    setCorrlationId(_instance, _logicalDevice, VK_OBJECT_TYPE_INSTANCE, "Instance: testVulkan");
    setCorrlationId(_logicalDevice, _logicalDevice, VK_OBJECT_TYPE_DEVICE, "Logic Device");

    volkLoadDevice(_logicalDevice);
}

void VkContext::Impl::cacheCommandQueue()
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

void VkContext::Impl::createVMA()
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

void VkContext::Impl::createSwapChain()
{
    const auto selectedPhysicalDevice = _selectedPhysicalDevice;
    const auto graphicsComputeQueueFamilyIndex = _graphicsComputeQueueFamilyIndex;
    const auto presentQueueFamilyIndex = _presentQueueFamilyIndex;
    const auto surfaceKHR = _surface;
    const auto logicalDevice = _logicalDevice;

    // for gamma correction, non-linear color space
    ASSERT(_surfaceFormats.end() != std::find_if(_surfaceFormats.begin(), _surfaceFormats.end(),
                                                 [&](const VkSurfaceFormatKHR &format)
                                                 {
                                                     return format.format == _swapChainFormat &&
                                                            format.colorSpace == _colorspace;
                                                 }),
           "swapChainFormat is not supported");

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selectedPhysicalDevice, surfaceKHR,
                                              &surfaceCapabilities);

    const bool presentationQueueIsShared =
        graphicsComputeQueueFamilyIndex == presentQueueFamilyIndex;

    std::array<uint32_t, 2> familyIndices{graphicsComputeQueueFamilyIndex,
                                          presentQueueFamilyIndex};
    const uint32_t swapChainImageCount =
        std::clamp(surfaceCapabilities.minImageCount + 1,
                   surfaceCapabilities.minImageCount,
                   surfaceCapabilities.maxImageCount);
    VkSurfaceTransformFlagBitsKHR pretransformFlag;

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
    pretransformFlag = surfaceCapabilities.currentTransform;

    log(Level::Info, "Creating swapchain...");
    log(Level::Info, "w: ", _swapChainExtent.width);
    log(Level::Info, "h: ", _swapChainExtent.height);
    log(Level::Info, "swapChainImageCount: ", swapChainImageCount);

    VkSwapchainCreateInfoKHR swapchain = {};
    swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain.surface = surfaceKHR;
    swapchain.minImageCount = swapChainImageCount;
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
    swapchain.preTransform = pretransformFlag;
    // ignore alpha completely
    // not supported: VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR for this default swapchain surface
    swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // VK_PRESENT_MODE_FIFO_KHR always supported
    // VK_PRESENT_MODE_FIFO_KHR = Hard Vsync
    // This is always supported on Android phones
    swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain.oldSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(logicalDevice, &swapchain, nullptr, &_swapChain));
    setCorrlationId(_swapChain, logicalDevice, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "Swapchain");
}

void VkContext::Impl::createSwapChainImageView()
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

VkRenderPass VkContext::Impl::createSwapChainRenderPass()
{
    const auto logicalDevice = _logicalDevice;

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

    VkRenderPass swapChainRenderPass{VK_NULL_HANDLE};
    VK_CHECK(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &swapChainRenderPass));
    setCorrlationId(swapChainRenderPass, logicalDevice, VK_OBJECT_TYPE_RENDER_PASS, "Render pass: SwapChain");
    return swapChainRenderPass;
}

VkFramebuffer VkContext::Impl::createFramebuffer(
    const std::string &name,
    VkRenderPass renderPass,
    const std::vector<VkImageView> &colors,
    const VkImageView depth,
    const VkImageView stencil,
    uint32_t width,
    uint32_t height)
{
    auto logicalDevice = _logicalDevice;

    std::vector<VkImageView> imageViews;
    for (const auto c : colors)
    {
        imageViews.push_back(c);
    }
    if (depth)
    {
        imageViews.push_back(depth);
    }
    if (stencil)
    {
        imageViews.push_back(stencil);
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = imageViews.size();
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;
    framebufferInfo.pAttachments = imageViews.data();

    VkFramebuffer fbo;
    VK_CHECK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr,
                                 &fbo));
    setCorrlationId(fbo, _logicalDevice, VK_OBJECT_TYPE_FRAMEBUFFER, name);
    return fbo;
}

std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> VkContext::Impl::createGraphicsCommandBuffers(
    const std::string &name,
    uint32_t count,
    uint32_t inflightCount,
    VkFenceCreateFlags flags)
{
    return this->createCommandBuffers(name, count, inflightCount, flags,
                                      _graphicsComputeQueueFamilyIndex,
                                      _graphicsQueue);
}

std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> VkContext::Impl::createTransferCommandBuffers(
    const std::string &name,
    uint32_t count,
    uint32_t inflightCount,
    VkFenceCreateFlags flags)
{
    return this->createCommandBuffers(name, count, inflightCount, flags,
                                      _transferQueueFamilyIndex,
                                      _transferQueue);
}

std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> VkContext::Impl::createCommandBuffers(
    const std::string &name,
    uint32_t count,
    uint32_t inflightCount,
    VkFenceCreateFlags flags,
    uint32_t queueFamilyIndex,
    VkQueue queue)
{
    ASSERT(inflightCount == 0 || count == inflightCount,
           "inflightCount must either be 0 or equal commandbuffer count");

    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> res;

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFence> inFlightFences;

    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    // since we want to record a command buffer every frame, so we want to be able to
    // reset and record over it
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    VK_CHECK(vkCreateCommandPool(_logicalDevice, &poolInfo, nullptr, &commandPool));

    commandBuffers.resize(count);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = commandBuffers.size();
    VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &allocInfo, commandBuffers.data()));

    inFlightFences.resize(count, VK_NULL_HANDLE);
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = flags;

    for (size_t i = 0; i < inflightCount; ++i)
    {
        VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]));
    }

    res.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        res.emplace_back(make_tuple(commandPool, commandBuffers[i], inFlightFences[i]));
    }
    return res;
}

std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> VkContext::Impl::createBuffer(
    const std::string &name,
    VkDeviceSize bufferSizeInBytes,
    VkBufferUsageFlags bufferUsageFlag,
    VkSharingMode bufferSharingMode,
    VmaAllocationCreateFlags memoryAllocationFlag,
    VkMemoryPropertyFlags requiredMemoryProperties,
    VkMemoryPropertyFlags preferredMemoryProperties,
    VmaMemoryUsage memoryUsage)
{
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo allocationInfo;
    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSizeInBytes,
        .usage = bufferUsageFlag,
        .sharingMode = bufferSharingMode,
    };

    const VmaAllocationCreateInfo bufferMemoryAllocationCreateInfo = {
        .flags = memoryAllocationFlag,
        .usage = memoryUsage,
        .requiredFlags = requiredMemoryProperties,
        .preferredFlags = preferredMemoryProperties,
    };

    VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo,
                             &bufferMemoryAllocationCreateInfo,
                             &buffer,
                             &allocation, nullptr));
    vmaGetAllocationInfo(_vmaAllocator, allocation, &allocationInfo);
    return make_tuple(buffer, allocation, allocationInfo);
}

std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> VkContext::Impl::createPersistentBuffer(
    const std::string &name,
    VkDeviceSize bufferSizeInBytes,
    VkBufferUsageFlags bufferUsageFlag)
{
    return createBuffer(
        name,
        bufferSizeInBytes,
        bufferUsageFlag,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
}

std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> VkContext::Impl::createStagingBuffer(
    const std::string &name,
    VkDeviceSize bufferSizeInBytes)
{
    return createBuffer(
        name,
        bufferSizeInBytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);
}

std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> VkContext::Impl::createDeviceLocalBuffer(
    const std::string &name,
    VkDeviceSize bufferSizeInBytes,
    VkBufferUsageFlags bufferUsageFlag)
{
    return createBuffer(
        name,
        bufferSizeInBytes,
        bufferUsageFlag,
        VK_SHARING_MODE_EXCLUSIVE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
}

std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> VkContext::Impl::createImage(
    const std::string &name,
    VkImageType imageType,
    VkFormat format,
    VkExtent3D extent,
    uint32_t textureMipLevelCount,
    uint32_t textureLayersCount,
    VkSampleCountFlagBits textureMultiSampleCount,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memoryFlags,
    bool generateMips)
{
    if (generateMips)
    {
        textureMipLevelCount = getMipLevelsCount(extent.width, extent.height);
    }
    ASSERT(!(textureMipLevelCount > 1 && textureMultiSampleCount != VK_SAMPLE_COUNT_1_BIT), "MSAA cannot have mipmaps");

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = imageType;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = textureMipLevelCount;
    imageCreateInfo.arrayLayers = textureLayersCount;
    imageCreateInfo.samples = textureMultiSampleCount;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = extent;
    // no need for VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, cpu does not need access
    const VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                     ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
                     : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .priority = 1.0f,
    };
    VkImage image;
    VmaAllocation imageAllocation;
    VmaAllocationInfo imageAllocationInfo;
    VkImageView imageView;
    VK_CHECK(vmaCreateImage(_vmaAllocator, &imageCreateInfo, &allocCreateInfo, &image,
                            &imageAllocation, nullptr));
    vmaGetAllocationInfo(_vmaAllocator, imageAllocation, &imageAllocationInfo);
    // image view
    VkImageViewCreateInfo imageViewInfo = {};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.viewType = getImageViewType(imageType);
    imageViewInfo.format = format;
    // subresource range could limit miplevel and layer ranges, here all are open to access
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = textureLayersCount;
    imageViewInfo.subresourceRange.levelCount = textureMipLevelCount;
    imageViewInfo.image = image;
    VK_CHECK(vkCreateImageView(_logicalDevice, &imageViewInfo, nullptr, &imageView));

    return make_tuple(image, imageView, imageAllocation, imageAllocationInfo, textureMipLevelCount,
                      extent, format);
}

std::tuple<VkSampler> VkContext::Impl::createSampler(const std::string &name)
{
    VkSampler sampler;
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 10.f;
    // Enable anisotropic filtering
    if (VkContext::sPhysicalDeviceFeatures2.features.samplerAnisotropy)
    {
        // Use max. level of anisotropy for this example
        samplerCreateInfo.maxAnisotropy = _physicalDevicesProp1.limits.maxSamplerAnisotropy;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
    }
    else
    {
        samplerCreateInfo.maxAnisotropy = 1.0;
        samplerCreateInfo.anisotropyEnable = VK_FALSE;
    }
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &sampler));
    setCorrlationId(sampler, _logicalDevice, VK_OBJECT_TYPE_SAMPLER, name);
    return make_tuple(sampler);
}

std::vector<VkDescriptorSetLayout> VkContext::Impl::createDescriptorSetLayout(
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> &setBindings)
{
    std::vector<VkDescriptorSetLayout> layouts;
    layouts.reserve(setBindings.size());

    // Descriptor binding flag VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT:
    // This flag indicates that descriptor set does not need to have valid descriptors in them
    // as long as the invalid descriptors are not accessed during shader execution.
    constexpr VkDescriptorBindingFlags flagsToEnable = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    for (const auto &setBinding : setBindings)
    {
        std::vector<VkDescriptorBindingFlags> bindFlags(setBinding.size(), flagsToEnable);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(bindFlags.size()),
            .pBindingFlags = bindFlags.data(),
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = setBinding.size();
        layoutInfo.pBindings = setBinding.data();
#if defined(_WIN32)
        layoutInfo.pNext = &extendedInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
#endif
        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr,
                                             &descriptorSetLayout));
        layouts.push_back(std::move(descriptorSetLayout));
    }
    return layouts;
}

void VkContext::Impl::writeBuffer(
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &deviceLocalBuffer,
    const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer,
    const void *rawData,
    uint32_t sizeInBytes,
    uint32_t srcOffset,
    uint32_t dstOffset)
{
    const auto stagingBufferHandle = std::get<0>(stagingBuffer);
    const auto deviceLocalBufferHandle = std::get<0>(deviceLocalBuffer);
    const auto vmaStagingImageBufferAllocation = std::get<1>(stagingBuffer);
    const auto cmdBufferHandle = std::get<1>(cmdBuffer);

    // copy vb from host to device, region
    void *mappedMemory{nullptr};
    VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingImageBufferAllocation, &mappedMemory));
    memcpy(mappedMemory, rawData, sizeInBytes);
    vmaUnmapMemory(_vmaAllocator, vmaStagingImageBufferAllocation);
    // cmd to copy from staging to device
    VkBufferCopy region{.srcOffset = srcOffset,
                        .dstOffset = dstOffset,
                        .size = sizeInBytes};
    vkCmdCopyBuffer(cmdBufferHandle, stagingBufferHandle, deviceLocalBufferHandle, 1, &region);
}

void VkContext::Impl::writeImage(
    const std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> &image,
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
    const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer,
    void *rawData)
{
    const auto imageHandle = std::get<0>(image);
    const auto textureMipLevelCount = std::get<4>(image);
    const auto extent = std::get<5>(image);
    const auto stagingBufferHandle = std::get<0>(stagingBuffer);
    const auto vmaStagingImageBufferAllocation = std::get<1>(stagingBuffer);
    const auto cmdBufferHandle = std::get<1>(cmdBuffer);

    void *imageDataPtr{nullptr};
    // format: VK_FORMAT_R8G8B8A8_UNORM took 4 bytes
    const auto imageDataSizeInBytes = get3DImageSizeInBytes(extent, VK_FORMAT_R8G8B8A8_UNORM);
    VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingImageBufferAllocation, &imageDataPtr));
    memcpy(imageDataPtr, rawData, imageDataSizeInBytes);
    vmaUnmapMemory(_vmaAllocator, vmaStagingImageBufferAllocation);

    // image layout from undefined to write dst
    // transition layout
    // barrier based on mip level, array layers
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = textureMipLevelCount;
    subresourceRange.layerCount = 1;

    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = imageHandle;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE; // 0: VK_ACCESS_NONE
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: written into
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkCmdPipelineBarrier(
        cmdBufferHandle,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    // now image layout(usage) is writable
    // staging buffer to device-local(image is device local memory)
    VkBufferImageCopy bufferCopyRegion = {};
    // mipmap level0: original copy
    bufferCopyRegion.bufferOffset = 0;
    // could be depth, stencil and color
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageOffset.x = bufferCopyRegion.imageOffset.y =
        bufferCopyRegion.imageOffset.z = 0;

    // write to the image mipmap level0
    bufferCopyRegion.imageExtent.width = extent.width;
    bufferCopyRegion.imageExtent.height = extent.height;
    bufferCopyRegion.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(
        cmdBufferHandle,
        stagingBufferHandle,
        imageHandle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion);
}

void VkContext::Impl::generateMipmaps(
    const std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> &image,
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
    const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer)
{
    const auto imageHandle = std::get<0>(image);
    const auto textureMipLevelCount = std::get<4>(image);
    const auto extent = std::get<5>(image);
    const auto format = std::get<6>(image);
    const auto cmdBufferHandle = std::get<1>(cmdBuffer);

    // generate mipmaps
    // sample: texturemipmapgen
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(_selectedPhysicalDevice, format, &formatProperties);
    ASSERT(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "Selected Physical Device cannot generate mipmaps");

    // make sure write to level0 is completed
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = imageHandle;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    int32_t w = extent.width;
    int32_t h = extent.height;
    for (uint32_t i = 1; i <= textureMipLevelCount; ++i)
    {
        // Prepare current mip level as image blit source for next level
        imageMemoryBarrier.subresourceRange.baseMipLevel = i - 1;
        vkCmdPipelineBarrier(
            cmdBufferHandle,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);
        // level0 write, barrier, level0 read, level 1write, barrier
        // level1 read, ....
        if (i == textureMipLevelCount)
        {
            break;
        }
        const int32_t newW = w > 1 ? w >> 1 : w;
        const int32_t newH = h > 1 ? h >> 1 : h;

        VkImageBlit imageBlit{};
        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.srcSubresource.baseArrayLayer = 0;
        imageBlit.srcSubresource.mipLevel = i - 1;
        imageBlit.srcOffsets[0].x = imageBlit.srcOffsets[0].y = imageBlit.srcOffsets[0].z = 0;
        imageBlit.srcOffsets[1].x = w;
        imageBlit.srcOffsets[1].y = h;
        imageBlit.srcOffsets[1].z = 1;

        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.dstSubresource.layerCount = 1;
        imageBlit.srcSubresource.baseArrayLayer = 0;
        imageBlit.dstSubresource.mipLevel = i;
        imageBlit.dstOffsets[1].x = newW;
        imageBlit.dstOffsets[1].y = newH;
        imageBlit.dstOffsets[1].z = 1;

        vkCmdBlitImage(cmdBufferHandle,
                       imageHandle,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       imageHandle,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &imageBlit,
                       VK_FILTER_LINEAR);
        w = newW;
        h = newH;
    }
    // all mip layers are in TRANSFER_SRC --> SHADER_READ
    const VkImageMemoryBarrier convertToShaderReadBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = imageHandle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = textureMipLevelCount,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },

    };
    vkCmdPipelineBarrier(cmdBufferHandle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr,
                         1, &convertToShaderReadBarrier);
}

VkPhysicalDeviceFeatures VkContext::sPhysicalDeviceFeatures = {

};

VkPhysicalDeviceFeatures2 VkContext::sPhysicalDeviceFeatures2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .features = sPhysicalDeviceFeatures,
};

VkPhysicalDeviceVulkan11Features VkContext::sEnable11Features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
};

VkPhysicalDeviceVulkan12Features VkContext::sEnable12Features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
};

VkPhysicalDeviceVulkan13Features VkContext::sEnable13Features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
};

VkPhysicalDeviceAccelerationStructureFeaturesKHR VkContext::sAccelStructFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
};

VkPhysicalDeviceRayTracingPipelineFeaturesKHR VkContext::sRayTracingPipelineFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
};

VkPhysicalDeviceRayQueryFeaturesKHR VkContext::sRayQueryFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
};

VkPhysicalDeviceFragmentDensityMapFeaturesEXT VkContext::sFragmentDensityMapFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
};

VkContext::VkContext(const Window &window,
                     const std::vector<const char *> &instanceValidationLayers,
                     const std::set<std::string> &instanceExtensions,
                     const std::vector<const char *> deviceExtensions)
{
    _pimpl = std::make_unique<Impl>(
        *this,
        window,
        instanceValidationLayers,
        instanceExtensions,
        deviceExtensions);
}

VkContext::~VkContext()
{
}

void VkContext::createSwapChain()
{
    _pimpl->createSwapChain();
    _pimpl->createSwapChainImageView();
}

VkRenderPass VkContext::createSwapChainRenderPass()
{
    return _pimpl->createSwapChainRenderPass();
}

VkFramebuffer VkContext::createFramebuffer(
    const std::string &name,
    VkRenderPass renderPass,
    const std::vector<VkImageView> &colors,
    const VkImageView depth,
    const VkImageView stencil,
    uint32_t width,
    uint32_t height)
{
    return _pimpl->createFramebuffer(name, renderPass, colors, depth, stencil, width, height);
}

std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> VkContext::createGraphicsCommandBuffers(
    const std::string &name,
    uint32_t count,
    uint32_t inflightCount,
    VkFenceCreateFlags flags)
{
    return _pimpl->createGraphicsCommandBuffers(name, count, inflightCount, flags);
}

std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> VkContext::createTransferCommandBuffers(
    const std::string &name,
    uint32_t count,
    uint32_t inflightCount,
    VkFenceCreateFlags flags)
{
    return _pimpl->createTransferCommandBuffers(name, count, inflightCount, flags);
}

std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> VkContext::createPersistentBuffer(
    const std::string &name,
    VkDeviceSize bufferSizeInBytes,
    VkBufferUsageFlags bufferUsageFlag)
{
    return _pimpl->createPersistentBuffer(name, bufferSizeInBytes, bufferUsageFlag);
}

std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> VkContext::createStagingBuffer(
    const std::string &name,
    VkDeviceSize bufferSizeInBytes)
{
    return _pimpl->createStagingBuffer(name, bufferSizeInBytes);
}

std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> VkContext::createDeviceLocalBuffer(
    const std::string &name,
    VkDeviceSize bufferSizeInBytes,
    VkBufferUsageFlags bufferUsageFlag)
{
    return _pimpl->createDeviceLocalBuffer(name, bufferSizeInBytes, bufferUsageFlag);
}

std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> VkContext::createImage(
    const std::string &name,
    VkImageType imageType,
    VkFormat format,
    VkExtent3D extent,
    uint32_t textureMipLevelCount,
    uint32_t textureLayersCount,
    VkSampleCountFlagBits textureMultiSampleCount,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memoryFlags,
    bool generateMips)
{
    return _pimpl->createImage(name, imageType, format, extent, textureMipLevelCount,
                               textureLayersCount, textureMultiSampleCount, usage, memoryFlags, generateMips);
}

std::tuple<VkSampler> VkContext::createSampler(const std::string &name)
{
    return _pimpl->createSampler(name);
}

std::vector<VkDescriptorSetLayout> VkContext::createDescriptorSetLayout(
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> &setBindings)
{
    return _pimpl->createDescriptorSetLayout(setBindings);
}

void VkContext::writeBuffer(
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &deviceLocalBuffer,
    const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer,
    const void *rawData,
    uint32_t sizeInBytes,
    uint32_t srcOffset,
    uint32_t dstOffset)
{
    return _pimpl->writeBuffer(stagingBuffer, deviceLocalBuffer, cmdBuffer, rawData, sizeInBytes, srcOffset, dstOffset);
}

void VkContext::writeImage(
    const std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> &image,
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
    const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer,
    void *rawData)
{
    return _pimpl->writeImage(image, stagingBuffer, cmdBuffer, rawData);
}

void VkContext::generateMipmaps(
    const std::tuple<VkImage, VkImageView, VmaAllocation, VmaAllocationInfo, uint32_t, VkExtent3D, VkFormat> &image,
    const std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> &stagingBuffer,
    const std::tuple<VkCommandPool, VkCommandBuffer, VkFence> &cmdBuffer)
{
    return _pimpl->generateMipmaps(image, stagingBuffer, cmdBuffer);
}

VkInstance VkContext::getInstance() const
{
    return _pimpl->getInstance();
}

VkDevice VkContext::getLogicDevice() const
{
    return _pimpl->getLogicDevice();
}

VmaAllocator VkContext::getVmaAllocator() const
{
    return _pimpl->getVmaAllocator();
}

VkQueue VkContext::getGraphicsQueue() const
{
    return _pimpl->getGraphicsQueue();
}

VkQueue VkContext::getPresentationQueue() const
{
    return _pimpl->getPresentationQueue();
}

VkPhysicalDevice VkContext::getSelectedPhysicalDevice() const
{
    return _pimpl->getSelectedPhysicalDevice();
}

VkPhysicalDeviceProperties VkContext::getSelectedPhysicalDeviceProp() const
{
    return _pimpl->getSelectedPhysicalDeviceProp();
}

VkSurfaceKHR VkContext::getSurfaceKHR() const
{
    return _pimpl->getSurfaceKHR();
}

uint32_t VkContext::getGraphicsComputeQueueFamilyIndex() const
{
    return _pimpl->getGraphicsComputeQueueFamilyIndex();
}

uint32_t VkContext::getPresentQueueFamilyIndex() const
{
    return _pimpl->getPresentQueueFamilyIndex();
}

VkPhysicalDeviceVulkan12Features VkContext::getVk12FeatureCaps() const
{
    return _pimpl->getVk12FeatureCaps();
}

VkSwapchainKHR VkContext::getSwapChain() const
{
    return _pimpl->getSwapChain();
}

VkExtent2D VkContext::getSwapChainExtent() const
{
    return _pimpl->getSwapChainExtent();
}

const std::vector<VkImageView> &VkContext::getSwapChainImageViews() const
{
    return _pimpl->getSwapChainImageViews();
}
