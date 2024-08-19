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
        // vmaDestroyAllocator(_vmaAllocator);
        vkDestroyDevice(_logicalDevice, nullptr);
        vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyInstance(_instance, nullptr);
    }

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