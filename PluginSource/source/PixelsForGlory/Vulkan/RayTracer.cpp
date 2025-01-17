#include "RayTracer.h"

namespace PixelsForGlory
{
    RayTracerAPI* CreateRayTracerAPI_Vulkan()
    {
        return &Vulkan::RayTracer::Instance();
    }
}

namespace PixelsForGlory::Vulkan
{
    /// <summary>
    /// Resolve properties and queues required for ray tracing
    /// </summary>
    /// <param name="physicalDevice"></param>
    void ResolvePropertiesAndQueues_RayTracer(VkPhysicalDevice physicalDevice) {

        // find our queues
        const VkQueueFlagBits askingFlags[3] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT };
        uint32_t queuesIndices[2] = { ~0u, ~0u };

        // Call once to get the count
        uint32_t queueFamilyPropertyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);

        // Call again to populate vector
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

        // Locate which queueFamilyProperties index 
        for (size_t i = 0; i < 2; ++i) {
            const VkQueueFlagBits flag = askingFlags[i];
            uint32_t& queueIdx = queuesIndices[i];

            if (flag == VK_QUEUE_TRANSFER_BIT) {
                for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
                    if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                        !(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                        queueIdx = j;
                        break;
                    }
                }
            }

            // If an index wasn't return by the above, just find any queue that supports the flag
            if (queueIdx == ~0u) {
                for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
                    if (queueFamilyProperties[j].queueFlags & flag) {
                        queueIdx = j;
                        break;
                    }
                }
            }
        }

        if (queuesIndices[0] == ~0u || queuesIndices[1] == ~0u) {
            PFG_EDITORLOGERROR("Could not find queues to support all required flags");
            return;
        }

        PixelsForGlory::Vulkan::RayTracer::Instance().graphicsQueueFamilyIndex_ = queuesIndices[0];
        PixelsForGlory::Vulkan::RayTracer::Instance().transferQueueFamilyIndex_ = queuesIndices[1];

        PFG_EDITORLOG("Queues indices successfully reoslved");

        // Get the ray tracing pipeline properties, which we'll need later on in the sample
        PixelsForGlory::Vulkan::RayTracer::Instance().rayTracingProperties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 physicalDeviceProperties = { };
        physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        physicalDeviceProperties.pNext = &PixelsForGlory::Vulkan::RayTracer::Instance().rayTracingProperties_;

        PFG_EDITORLOG("Getting physical device properties");
        vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties);

        // Get memory properties
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &PixelsForGlory::Vulkan::RayTracer::Instance().physicalDeviceMemoryProperties_);
    }

    VkResult CreateInstance_RayTracer(const VkInstanceCreateInfo* unityCreateInfo, const VkAllocationCallbacks* unityAllocator, VkInstance* instance)
    {
        // Copy ApplicationInfo from Unity and force it up to Vulkan 1.2
        VkApplicationInfo applicationInfo;
        applicationInfo.sType = unityCreateInfo->pApplicationInfo->sType;
        applicationInfo.apiVersion = VK_API_VERSION_1_2;
        applicationInfo.applicationVersion = unityCreateInfo->pApplicationInfo->applicationVersion;
        applicationInfo.engineVersion = unityCreateInfo->pApplicationInfo->engineVersion;
        applicationInfo.pApplicationName = unityCreateInfo->pApplicationInfo->pApplicationName;
        applicationInfo.pEngineName = unityCreateInfo->pApplicationInfo->pEngineName;
        applicationInfo.pNext = unityCreateInfo->pApplicationInfo->pNext;

        // Define Vulkan create information for instance
        VkInstanceCreateInfo createInfo;
        createInfo.pNext = nullptr;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &applicationInfo;

        // At time of writing this was 0, make sure Unity didn't change anything
        assert(unityCreateInfo->enabledLayerCount == 0);

        // Depending if we have validations layers defined, populate debug messenger create info
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        const char* layerName = "VK_LAYER_KHRONOS_validation";

        // TODO: debug flag
        if (/*_debug*/ false) {
            createInfo.enabledLayerCount = 1;
            createInfo.ppEnabledLayerNames = &layerName;

            debugCreateInfo = {};
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = debugCallback;

            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Gather extensions required for instaqnce
        std::vector<const char*> requiredExtensions = {
            VK_KHR_DISPLAY_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef WIN32
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #endif
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,

            /*VK_EXT_DEBUG_UTILS_EXTENSION_NAME*/  // Enable when debugging of Vulkan is needed
        };

        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
        VkResult result = vkCreateInstance(&createInfo, unityAllocator, instance);
        VK_CHECK("vkCreateInstance", result);

        if (result == VK_SUCCESS)
        {
            volkLoadInstance(*instance);
        }

        // Unity automatically picks up on VkDebugUtilsMessengerCreateInfoEXT
        // If for some reason it doesn't, we'd hook up a debug callback here
        //if (/*_debug*/ true) {
        //    if (CreateDebugUtilsMessengerEXT(*instance, &debugCreateInfo, nullptr, &PixelsForGlory::Vulkan::RayTracer::Instance().debugMessenger_) != VK_SUCCESS) 
        //    {
        //        PFG_EDITORLOGERROR("failed to set up debug messenger!");
        //    }
        //}

        return result;
    }

    VkResult CreateDevice_RayTracer(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* unityCreateInfo, const VkAllocationCallbacks* unityAllocator, VkDevice* device)
    {
        ResolvePropertiesAndQueues_RayTracer(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
        const float priority = 0.0f;

        //  Setup device queue
        VkDeviceQueueCreateInfo deviceQueueCreateInfo;
        deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo.pNext = nullptr;
        deviceQueueCreateInfo.flags = 0;
        deviceQueueCreateInfo.queueFamilyIndex = PixelsForGlory::Vulkan::RayTracer::Instance().graphicsQueueFamilyIndex_;
        deviceQueueCreateInfo.queueCount = 1;
        deviceQueueCreateInfo.pQueuePriorities = &priority;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);

        // Determine if we have individual queues that need to be added 
        if (PixelsForGlory::Vulkan::RayTracer::Instance().transferQueueFamilyIndex_ != PixelsForGlory::Vulkan::RayTracer::Instance().graphicsQueueFamilyIndex_) {
            deviceQueueCreateInfo.queueFamilyIndex = PixelsForGlory::Vulkan::RayTracer::Instance().transferQueueFamilyIndex_;
            deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
        }

        // Setup required features backwards for chaining
        VkPhysicalDeviceDescriptorIndexingFeatures physicalDeviceDescriptorIndexingFeatures = { };
        physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        physicalDeviceDescriptorIndexingFeatures.pNext = nullptr;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR physicalDeviceRayTracingPipelineFeatures = { };
        physicalDeviceRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        physicalDeviceRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        physicalDeviceRayTracingPipelineFeatures.pNext = &physicalDeviceDescriptorIndexingFeatures;

        VkPhysicalDeviceBufferDeviceAddressFeatures physicalDeviceBufferDeviceAddressFeatures = { };
        physicalDeviceBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        physicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
        physicalDeviceBufferDeviceAddressFeatures.pNext = &physicalDeviceRayTracingPipelineFeatures;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeatures = { };
        physicalDeviceAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        physicalDeviceAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
        physicalDeviceAccelerationStructureFeatures.pNext = &physicalDeviceBufferDeviceAddressFeatures;

        VkPhysicalDeviceFeatures2 deviceFeatures = { };
        deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext = &physicalDeviceAccelerationStructureFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures); // enable all the features our GPU has

        // Setup extensions required for ray tracing.  Rebuild from Unity
        std::vector<const char*> requiredExtensions = {
            // Required by Unity3D
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
            VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
            VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
            VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
            VK_KHR_MAINTENANCE1_EXTENSION_NAME,
            VK_KHR_MAINTENANCE2_EXTENSION_NAME,
            VK_KHR_MULTIVIEW_EXTENSION_NAME,
            VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
            VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
            VK_EXT_HDR_METADATA_EXTENSION_NAME,
            VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,

            // Required by Raytracing
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,

            // Required by SPRIV 1.4
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,

            // Required by acceleration structure
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        };

        for (auto const& ext : requiredExtensions) {
            PFG_EDITORLOG("Enabling extension: " + std::string(ext));
        }

        VkDeviceCreateInfo deviceCreateInfo;
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures;
        deviceCreateInfo.flags = VK_NO_FLAGS;
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
        deviceCreateInfo.enabledLayerCount = 0;
        deviceCreateInfo.ppEnabledLayerNames = nullptr;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();
        deviceCreateInfo.pEnabledFeatures = nullptr; // Must be null since pNext != nullptr

        // Inject parts of Unity's VkDeviceCreateInfo just in case
        deviceCreateInfo.flags = unityCreateInfo->flags;
        deviceCreateInfo.enabledLayerCount = unityCreateInfo->enabledLayerCount;
        deviceCreateInfo.ppEnabledLayerNames = unityCreateInfo->ppEnabledLayerNames;

        VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, unityAllocator, device);


        // get our queues handles with our new logical device!
        PFG_EDITORLOG("Getting device queues");
        vkGetDeviceQueue(*device, PixelsForGlory::Vulkan::RayTracer::Instance().graphicsQueueFamilyIndex_, 0, &PixelsForGlory::Vulkan::RayTracer::Instance().graphicsQueue_);
        vkGetDeviceQueue(*device, PixelsForGlory::Vulkan::RayTracer::Instance().transferQueueFamilyIndex_, 0, &PixelsForGlory::Vulkan::RayTracer::Instance().transferQueue_);

        if (result == VK_SUCCESS)
        {
            Vulkan::RayTracer::CreateDeviceSuccess = true;
        }

        return result;
    }

    VkDevice RayTracer::NullDevice = VK_NULL_HANDLE;
    bool RayTracer::CreateDeviceSuccess = false;

    RayTracer::RayTracer()
        : graphicsInterface_(nullptr)
        , graphicsQueueFamilyIndex_(0)
        , transferQueueFamilyIndex_(0)
        , graphicsQueue_(VK_NULL_HANDLE)
        , transferQueue_(VK_NULL_HANDLE)
        , graphicsCommandPool_(VK_NULL_HANDLE)
        , transferCommandPool_(VK_NULL_HANDLE)
        , physicalDeviceMemoryProperties_(VkPhysicalDeviceMemoryProperties())
        , rayTracingProperties_(VkPhysicalDeviceRayTracingPipelinePropertiesKHR())
        , device_(NullDevice)
        , alreadyPrepared_(false)
        , rebuildTlas_(true)
        , updateTlas_(false)
        , tlas_(RayTracerAccelerationStructure())
        , descriptorPool_(VK_NULL_HANDLE)
        , sceneBufferInfo_(VkDescriptorBufferInfo())
        , pipelineLayout_(VK_NULL_HANDLE)
        , pipeline_(VK_NULL_HANDLE)
        , debugMessenger_(VK_NULL_HANDLE)
    {}

    void RayTracer::InitializeFromUnityInstance(IUnityGraphicsVulkan* graphicsInterface)
    {
        graphicsInterface_ = graphicsInterface;
        device_ = graphicsInterface_->Instance().device;

        // Setup one off command pools
        CreateCommandPool(graphicsQueueFamilyIndex_, graphicsCommandPool_);
        CreateCommandPool(transferQueueFamilyIndex_, transferCommandPool_);  
    }

    void RayTracer::Shutdown()
    {
        if (debugMessenger_ != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(graphicsInterface_->Instance().instance, debugMessenger_, nullptr);
            debugMessenger_ = VK_NULL_HANDLE;
        }

        if (graphicsCommandPool_ != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device_, graphicsCommandPool_, nullptr);
            graphicsCommandPool_ = VK_NULL_HANDLE;
        }

        if (transferCommandPool_ != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device_, transferCommandPool_, nullptr);
            transferCommandPool_ = VK_NULL_HANDLE;
        }

        for (auto itr = renderTargets_.begin(); itr != renderTargets_.end(); ++itr)
        {
            auto& renderTarget = (*itr).second;

            renderTarget->stagingImage.Destroy();
            renderTarget->cameraData.Destroy();

            if (renderTarget->descriptorSets.size() > 0)
            {
                vkFreeDescriptorSets(device_, descriptorPool_, DESCRIPTOR_SET_SIZE, renderTarget->descriptorSets.data());
            }
            renderTarget->descriptorSets.clear();

            renderTarget.release();
        }
        renderTargets_.clear();
        

        for (auto itr = sharedMeshesPool_.pool_begin(); itr != sharedMeshesPool_.pool_end(); ++itr)
        {
            auto const& mesh = (*itr);

            mesh->vertexBuffer.Destroy();
            mesh->indexBuffer.Destroy();
            
            if (mesh->blas.accelerationStructure != VK_NULL_HANDLE)
            {
                vkDestroyAccelerationStructureKHR(device_, mesh->blas.accelerationStructure, nullptr);
                mesh->blas.accelerationStructure = VkAccelerationStructureKHR();
                mesh->blas.buffer.Destroy();
                mesh->blas.deviceAddress = 0;
            }
        }

        for (auto i = sharedMeshAttributesPool_.pool_begin(); i != sharedMeshAttributesPool_.pool_end(); ++i)
        {
            (*i).Destroy();
        }

        for (auto i = sharedMeshFacesPool_.pool_begin(); i != sharedMeshFacesPool_.pool_end(); ++i)
        {
            (*i).Destroy();
        }

        instancesAccelerationStructuresBuffer_.Destroy();

        if (tlas_.accelerationStructure != VK_NULL_HANDLE)
        {
            vkDestroyAccelerationStructureKHR(device_, tlas_.accelerationStructure, nullptr);
        }
        tlas_.buffer.Destroy();

        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }

        shaderBindingTable_.Destroy();

        if (pipeline_ != VK_NULL_HANDLE) 
        {
            vkDestroyPipeline(device_, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }

        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
            pipelineLayout_ = VK_NULL_HANDLE;
        }

        for (auto descriptorSetLayout : descriptorSetLayouts_)
        {
            vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, nullptr);
        }
        descriptorSetLayouts_.clear();


    }

#pragma region RayTracerAPI
    bool RayTracer::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) 
    {
        switch (type)
        {
        case kUnityGfxDeviceEventInitialize:
            {
                PFG_EDITORLOG("Processing kUnityGfxDeviceEventInitialize...");

                auto graphicsInterface = interfaces->Get<IUnityGraphicsVulkan>();

                UnityVulkanPluginEventConfig eventConfig;
                eventConfig.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_Allow;
                eventConfig.renderPassPrecondition = kUnityVulkanRenderPass_EnsureOutside;
                eventConfig.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission | kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
                graphicsInterface->ConfigureEvent(1, &eventConfig);

                if (CreateDeviceSuccess == false)
                {
                    PFG_EDITORLOG("Ray Tracing Plugin initialization failed.  Check that plugin is loading at startup");
                    return false;
                }
                
                // Finish setting up RayTracer instance
                InitializeFromUnityInstance(graphicsInterface);

                // alternative way to intercept API
                //graphicsInterface_->InterceptVulkanAPI("vkCmdBeginRenderPass", (PFN_vkVoidFunction)Hook_vkCmdBeginRenderPass);
        }
            break;

        case kUnityGfxDeviceEventBeforeReset:
            {
                PFG_EDITORLOG("Processing kUnityGfxDeviceEventBeforeReset...");
            }
            break;

        case kUnityGfxDeviceEventAfterReset:
            {
                PFG_EDITORLOG("Processing kUnityGfxDeviceEventAfterReset...");
            }
            break;

        case kUnityGfxDeviceEventShutdown:
            {
                PFG_EDITORLOG("Processing kUnityGfxDeviceEventShutdown...");

                if (CreateDeviceSuccess == false)
                {
                    // Nothing to do
                    return false;
                }

                Shutdown();
            }
            break;
        }

        return true;
    }

    void RayTracer::SetShaderFolder(std::string shaderFolder)
    {
        shaderFolder_ = shaderFolder;
        

        if (shaderFolder_.back() != '/' || shaderFolder_.back() != '\\')
        {
            shaderFolder_ = shaderFolder_ + "/";
        }

        PFG_EDITORLOG("Shader folder set to: " + shaderFolder_);
    }

    int RayTracer::SetRenderTarget(int cameraInstanceId, int unityTextureFormat, int width, int height, void* textureHandle)
    {
        VkFormat vkFormat;
        switch (unityTextureFormat)
        {
        
            //// RenderTextureFormat.ARGB32
        //case 0:
        //    vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
        //    break;
        
        // TextureFormat.RGBA32
        case 4:
            vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        
        default:
            PFG_EDITORLOGERROR("Attempted to set an unsupported Unity texture format" + std::to_string(unityTextureFormat));
            return 0;
        }

        if (renderTargets_.find(cameraInstanceId) != renderTargets_.end())
        {
            auto& renderTarget = renderTargets_[cameraInstanceId];
                
            renderTarget->cameraData.Destroy();
            renderTarget->stagingImage.Destroy();
            
            if (renderTarget->descriptorSets.size() > 0)
            {
                vkFreeDescriptorSets(device_, descriptorPool_, static_cast<uint32_t>(renderTarget->descriptorSets.size()), renderTarget->descriptorSets.data());
            }

            renderTarget.release();
            renderTargets_.erase(cameraInstanceId);
        }

        auto renderTarget = std::make_unique<RayTracerRenderTarget>();

        renderTarget->format = vkFormat;
        renderTarget->extent.width = width;
        renderTarget->extent.height = height;
        renderTarget->extent.depth = 1;
        renderTarget->destination = textureHandle;
        
        renderTarget->stagingImage = Vulkan::Image(device_, physicalDeviceMemoryProperties_);

        if (renderTarget->stagingImage.Create(
            VK_IMAGE_TYPE_2D,
            renderTarget->format,
            renderTarget->extent,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != VK_SUCCESS) 
        {

            PFG_EDITORLOGERROR("Failed to create render image!");
            renderTarget->stagingImage.Destroy();
            return 0;
        }

        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if(renderTarget->stagingImage.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, renderTarget->format, range) != VK_SUCCESS) {
            PFG_EDITORLOGERROR("Failed to create render image view!");
            renderTarget->stagingImage.Destroy();
            return 0;
        }

        // Make sure descriptor sets are updated if a new render target has been created
        renderTarget->updateDescriptorSetsData = true;

        renderTargets_.insert(std::make_pair(cameraInstanceId, std::move(renderTarget)));

        return 1;
    }
        
    int RayTracer::GetSharedMeshIndex(int sharedMeshInstanceId) 
    { 
        for (auto itr = sharedMeshesPool_.in_use_begin(); itr != sharedMeshesPool_.in_use_end(); ++itr)
        {
            auto i = (*itr);
            if (sharedMeshesPool_[i]->sharedMeshInstanceId == sharedMeshInstanceId)
            {
                return i;
            }
        }
    
        return -1;    
    }

    int RayTracer::AddSharedMesh(int instanceId, float* verticesArray, float* normalsArray, float* uvsArray, int vertexCount, int* indicesArray, int indexCount) 
    { 
        // Check that this shared mesh hasn't been added yet
        for (auto itr = sharedMeshesPool_.in_use_begin(); itr != sharedMeshesPool_.in_use_end(); ++itr)
        {
            auto i = (*itr);
            if (sharedMeshesPool_[i]->sharedMeshInstanceId == instanceId)
            {
                return i;
            }
        }
        
        // We can only add tris, make sure the index count reflects this
        assert(indexCount % 3 == 0);
    
        auto sentMesh = std::make_unique<RayTracerMeshSharedData>();
        
        // Setup where we are going to store the shared mesh data and all data needed for shaders
        sentMesh->sharedMeshInstanceId = instanceId;
        sentMesh->vertexCount = vertexCount;
        sentMesh->indexCount = indexCount;

        sentMesh->vertexAttributeIndex = sharedMeshAttributesPool_.get_next_index();
        sentMesh->faceDataIndex = sharedMeshFacesPool_.get_next_index();
    
        Vulkan::Buffer& sentMeshAttributes = sharedMeshAttributesPool_[sentMesh->vertexAttributeIndex];
        Vulkan::Buffer& sentMeshFaces = sharedMeshFacesPool_[sentMesh->faceDataIndex];
    
        // Setup buffers
        bool success = true;
        if (sentMesh->vertexBuffer.Create(
                device_,
                physicalDeviceMemoryProperties_,
                sizeof(vec3) * sentMesh->vertexCount,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                Vulkan::Buffer::kDefaultMemoryPropertyFlags) 
            != VK_SUCCESS)
        {
            PFG_EDITORLOGERROR("Failed to create vertex buffer for shared mesh instance id " + std::to_string(instanceId));
            success = false;
        }
    
        if (sentMesh->indexBuffer.Create(
            device_,
            physicalDeviceMemoryProperties_,
            sizeof(uint32_t) * sentMesh->indexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            Vulkan::Buffer::kDefaultMemoryPropertyFlags))
        {
            PFG_EDITORLOGERROR("Failed to create index buffer for shared mesh instance id " + std::to_string(instanceId));
            success = false;
        }
    
        if (sentMeshAttributes.Create(
            device_,
                physicalDeviceMemoryProperties_,
                sizeof(ShaderVertexAttribute) * sentMesh->vertexCount,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                Vulkan::Buffer::kDefaultMemoryPropertyFlags)
            != VK_SUCCESS)
        {
            PFG_EDITORLOGERROR("Failed to create vertex attribute buffer for shared mesh instance id " + std::to_string(instanceId));
            success = false;
        }
    
    
        if (sentMeshFaces.Create(
            device_,
            physicalDeviceMemoryProperties_,
            sizeof(ShaderFace) * sentMesh->indexCount / 3,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            Vulkan::Buffer::kDefaultMemoryPropertyFlags)
            != VK_SUCCESS)
        {
            PFG_EDITORLOGERROR("Failed to create face buffer for shared mesh instance id " + std::to_string(instanceId));
            success = false;
        }
    
        
        if (!success)
        {
            return -1;
        }
    
        // Creating buffers was successful.  Move onto getting the data in there
        auto vertices = reinterpret_cast<vec3*>(sentMesh->vertexBuffer.Map());
        auto indices = reinterpret_cast<uint32_t*>(sentMesh->indexBuffer.Map());
        auto vertexAttributes = reinterpret_cast<ShaderVertexAttribute*>(sentMeshAttributes.Map()); 
        auto faces = reinterpret_cast<ShaderFace*>(sentMeshFaces.Map());
        
        // verticesArray and normalsArray are size vertexCount * 3 since they actually represent an array of vec3
        // uvsArray is size vertexCount * 2 since it actually represents an array of vec2
        for (int i = 0; i < vertexCount; ++i)
        {
            // Build for acceleration structure
            vertices[i].x = verticesArray[3 * i + 0];
            vertices[i].y = verticesArray[3 * i + 1];
            vertices[i].z = verticesArray[3 * i + 2];
            
            //PFG_EDITORLOG("vertex #" + std::to_string(i) + ": " + std::to_string(vertices[i].x) + ", " + std::to_string(vertices[i].y) + ", " + std::to_string(vertices[i].z));

            // Build for shader
            vertexAttributes[i].normal.x = normalsArray[3 * i + 0];
            vertexAttributes[i].normal.y = normalsArray[3 * i + 1];
            vertexAttributes[i].normal.z = normalsArray[3 * i + 2];
    
            vertexAttributes[i].uv.x = uvsArray[2 * i + 0];
            vertexAttributes[i].uv.y = uvsArray[2 * i + 1];
        }
        
        
        for (int i = 0; i < indexCount / 3; ++i)
        {
            // Build for acceleration structure
            indices[3 * i + 0] = static_cast<uint32_t>(indicesArray[3 * i + 0]);
            indices[3 * i + 1] = static_cast<uint32_t>(indicesArray[3 * i + 1]);
            indices[3 * i + 2] = static_cast<uint32_t>(indicesArray[3 * i + 2]);

            //PFG_EDITORLOG("face #" + std::to_string(i) + ": " + std::to_string(indices[3 * i + 0]) + ", " + std::to_string(indices[3 * i + 1]) + ", " + std::to_string(indices[3 * i + 2]));

            // Build for shader
            faces[i].index0 = static_cast<uint32_t>(indicesArray[3 * i + 0]);
            faces[i].index1 = static_cast<uint32_t>(indicesArray[3 * i + 1]);
            faces[i].index2 = static_cast<uint32_t>(indicesArray[3 * i + 2]);
        }
        
        sentMesh->vertexBuffer.Unmap();
        sentMesh->indexBuffer.Unmap();
        sentMeshAttributes.Unmap();
        sentMeshFaces.Unmap();

        // All done creating the data, get it added to the pool
        int sharedMeshIndex = sharedMeshesPool_.add(std::move(sentMesh));
    
        // Build blas here so we don't have to do it later
        BuildBlas(sharedMeshIndex);
    
        PFG_EDITORLOG("Added mesh (sharedMeshInstanceId: " + std::to_string(instanceId) + ")");
    
        return sharedMeshIndex;
    
    }

    int RayTracer::GetTlasInstanceIndex(int gameObjectInstanceId)
    {
        for (auto itr = meshInstancePool_.in_use_begin(); itr != meshInstancePool_.in_use_end(); ++itr)
        {
            auto index = *itr;
            if (meshInstancePool_[index]->gameObjectInstanceId == gameObjectInstanceId)
            {
                return index;
            }
        }

        return -1;
    }

    int RayTracer::AddTlasInstance(int gameObjectInstanceId, int sharedMeshIndex, float* l2wMatrix) 
    { 
        auto instance = std::make_unique<RayTracerMeshInstanceData>();

        instance->gameObjectInstanceId = gameObjectInstanceId;
        instance->sharedMeshIndex = sharedMeshIndex;
        FloatArrayToMatrix(l2wMatrix, instance->localToWorld);

        int index = meshInstancePool_.add(std::move(instance));

        PFG_EDITORLOG("Added mesh instance (sharedMeshIndex: " + std::to_string(sharedMeshIndex) + ")");

        // If we added an instance, we need to rebuild the tlas
        rebuildTlas_ = true;

        return index; 
    }

    void RayTracer::RemoveTlasInstance(int meshInstanceIndex) 
    {
        meshInstancePool_.remove(meshInstanceIndex);

        // If we added an instance, we need to rebuild the tlas
        rebuildTlas_ = true;
    }

    void RayTracer::BuildTlas() 
    {
        // If there is nothing to do, skip building the tlas
        if (rebuildTlas_ == false && updateTlas_ == false)
        {
            return;
        }

        bool update = updateTlas_;
        if (rebuildTlas_)
        {
            update = false;
        }

        if (meshInstancePool_.in_use_size() == 0)
        {
            // We have no instances, so there is nothing to build 
            return;
        }
     
        if (!update)
        {
            // Build instance buffer from scratch
            std::vector<VkAccelerationStructureInstanceKHR> instanceAccelerationStructures;
            instanceAccelerationStructures.resize(meshInstancePool_.in_use_size(), VkAccelerationStructureInstanceKHR{});

            // Gather instances
            uint32_t instanceAccelerationStructuresIndex = 0;
            for (auto i = meshInstancePool_.in_use_begin(); i != meshInstancePool_.in_use_end(); ++i)
            {
                auto instanceIndex = (*i);

                const auto& t = meshInstancePool_[instanceIndex]->localToWorld;
                VkTransformMatrixKHR transformMatrix = {
                    t[0][0], t[0][1], t[0][2], t[0][3],
                    t[1][0], t[1][1], t[1][2], t[1][3],
                    t[2][0], t[2][1], t[2][2], t[2][3]
                };

                /*PFG_EDITORLOG("Instance transform matrix loop: ");
                PFG_EDITORLOG(std::to_string(transformMatrix.matrix[0][0]) + ", " + std::to_string(transformMatrix.matrix[0][1]) + ", " + std::to_string(transformMatrix.matrix[0][2]) + ", " + std::to_string(transformMatrix.matrix[0][3]));
                PFG_EDITORLOG(std::to_string(transformMatrix.matrix[1][0]) + ", " + std::to_string(transformMatrix.matrix[1][1]) + ", " + std::to_string(transformMatrix.matrix[1][2]) + ", " + std::to_string(transformMatrix.matrix[1][3]));
                PFG_EDITORLOG(std::to_string(transformMatrix.matrix[2][0]) + ", " + std::to_string(transformMatrix.matrix[2][1]) + ", " + std::to_string(transformMatrix.matrix[2][2]) + ", " + std::to_string(transformMatrix.matrix[2][3]));*/

                auto sharedMeshIndex = meshInstancePool_[instanceIndex]->sharedMeshIndex;

                VkAccelerationStructureInstanceKHR& accelerationStructureInstance = instanceAccelerationStructures[instanceAccelerationStructuresIndex];
                accelerationStructureInstance.transform = transformMatrix;
                accelerationStructureInstance.instanceCustomIndex = instanceIndex;
                accelerationStructureInstance.mask = 0xFF;
                accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
                accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                accelerationStructureInstance.accelerationStructureReference = sharedMeshesPool_[meshInstancePool_[instanceIndex]->sharedMeshIndex]->blas.deviceAddress;

                /*PFG_EDITORLOG("VkAccelerationStructureInstanceKHR.transform: ");
                PFG_EDITORLOG(std::to_string(accelerationStructureInstance.transform.matrix[0][0]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[0][1]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[0][2]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[0][3]));
                PFG_EDITORLOG(std::to_string(accelerationStructureInstance.transform.matrix[1][0]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[1][1]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[1][2]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[1][3]));
                PFG_EDITORLOG(std::to_string(accelerationStructureInstance.transform.matrix[2][0]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[2][1]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[2][2]) + ", " + std::to_string(accelerationStructureInstance.transform.matrix[2][3]));*/

                // Consumed current index, advance
                ++instanceAccelerationStructuresIndex;
            }

            // Destroy anytihng if created before
            instancesAccelerationStructuresBuffer_.Destroy();

            instancesAccelerationStructuresBuffer_.Create(
                device_,
                physicalDeviceMemoryProperties_,
                instanceAccelerationStructures.size() * sizeof(VkAccelerationStructureInstanceKHR),
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                Vulkan::Buffer::kDefaultMemoryPropertyFlags);
            instancesAccelerationStructuresBuffer_.UploadData(instanceAccelerationStructures.data(), instancesAccelerationStructuresBuffer_.GetSize());

            /*auto instances = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(instancesAccelerationStructuresBuffer_.Map());
            for (int i = 0; i < meshInstancePool_.in_use_size(); ++i)
            {

                PFG_EDITORLOG("Instance transform matrix after upload: ");
                PFG_EDITORLOG(std::to_string(instances[i].transform.matrix[0][0]) + ", " + std::to_string(instances[i].transform.matrix[0][1]) + ", " + std::to_string(instances[i].transform.matrix[0][2]) + ", " + std::to_string(instances[i].transform.matrix[0][3]));
                PFG_EDITORLOG(std::to_string(instances[i].transform.matrix[1][0]) + ", " + std::to_string(instances[i].transform.matrix[1][1]) + ", " + std::to_string(instances[i].transform.matrix[1][2]) + ", " + std::to_string(instances[i].transform.matrix[1][3]));
                PFG_EDITORLOG(std::to_string(instances[i].transform.matrix[2][0]) + ", " + std::to_string(instances[i].transform.matrix[2][1]) + ", " + std::to_string(instances[i].transform.matrix[2][2]) + ", " + std::to_string(instances[i].transform.matrix[2][3]));

            }
            instancesAccelerationStructuresBuffer_.Unmap();*/
        }
        else
        {
            // Update transforms in buffer, should be the exact same layout

            auto instances = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(instancesAccelerationStructuresBuffer_.Map());

            // Gather instances
            uint32_t instanceAccelerationStructuresIndex = 0;
            for (auto i = meshInstancePool_.in_use_begin(); i != meshInstancePool_.in_use_end(); ++i)
            {
                auto instanceIndex = (*i);

                const auto& t = meshInstancePool_[instanceIndex]->localToWorld;
                VkTransformMatrixKHR transformMatrix = {
                    t[0][0], t[0][1], t[0][2], t[0][3],
                    t[1][0], t[1][1], t[1][2], t[1][3],
                    t[2][0], t[2][1], t[2][2], t[2][3]
                };

                //PFG_EDITORLOG("Instance transform matrix: ");
                //PFG_EDITORLOG(std::to_string(t[0][0]) + ", " + std::to_string(t[0][1]) + ", " + std::to_string(t[0][2]) + ", " + std::to_string(t[0][3]));
                //PFG_EDITORLOG(std::to_string(t[1][0]) + ", " + std::to_string(t[1][1]) + ", " + std::to_string(t[1][2]) + ", " + std::to_string(t[1][3]));
                //PFG_EDITORLOG(std::to_string(t[2][0]) + ", " + std::to_string(t[2][1]) + ", " + std::to_string(t[2][2]) + ", " + std::to_string(t[2][3]));

                instances[instanceAccelerationStructuresIndex].transform = transformMatrix;
                
                // Consumed current index, advance
                ++instanceAccelerationStructuresIndex;
            }
            instancesAccelerationStructuresBuffer_.Unmap();
        }

        //auto instances = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(instancesAccelerationStructuresBuffer_.Map());
        //for (int i = 0; i < meshInstancePool_.in_use_size(); ++i)
        //{
        //    
        //    PFG_EDITORLOG("Instance transform matrix: ");
        //        PFG_EDITORLOG(std::to_string(instances[i].transform.matrix[0][0]) + ", " + std::to_string(instances[i].transform.matrix[0][1]) + ", " + std::to_string(instances[i].transform.matrix[0][2]) + ", " + std::to_string(instances[i].transform.matrix[0][3]));
        //        PFG_EDITORLOG(std::to_string(instances[i].transform.matrix[1][0]) + ", " + std::to_string(instances[i].transform.matrix[1][1]) + ", " + std::to_string(instances[i].transform.matrix[1][2]) + ", " + std::to_string(instances[i].transform.matrix[1][3]));
        //        PFG_EDITORLOG(std::to_string(instances[i].transform.matrix[2][0]) + ", " + std::to_string(instances[i].transform.matrix[2][1]) + ", " + std::to_string(instances[i].transform.matrix[2][2]) + ", " + std::to_string(instances[i].transform.matrix[2][3]));
        //        
        //}
        //instancesAccelerationStructuresBuffer_.Unmap();

        // The top level acceleration structure contains (bottom level) instance as the input geometry
        VkAccelerationStructureGeometryInstancesDataKHR accelerationStructureGeometryInstancesData = {};
        accelerationStructureGeometryInstancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructureGeometryInstancesData.arrayOfPointers = VK_FALSE;
        accelerationStructureGeometryInstancesData.data.deviceAddress = instancesAccelerationStructuresBuffer_.GetBufferDeviceAddressConst().deviceAddress;

        VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructureGeometry.geometry.instances = accelerationStructureGeometryInstancesData;

        // Get the size requirements for buffers involved in the acceleration structure build process
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        // Number of instances
        uint32_t instancesCount = static_cast<uint32_t>(meshInstancePool_.in_use_size());

        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device_,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &instancesCount,
            &accelerationStructureBuildSizesInfo);

        if (!update)
        {
            tlas_.buffer.Destroy();

            // Create a buffer to hold the acceleration structure
            tlas_.buffer.Create(
                device_,
                physicalDeviceMemoryProperties_,
                accelerationStructureBuildSizesInfo.accelerationStructureSize,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                Vulkan::Buffer::kDefaultMemoryPropertyFlags);

            // Create the acceleration structure
            VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
            accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            accelerationStructureCreateInfo.buffer = tlas_.buffer.GetBuffer();
            accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
            accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            VK_CHECK("vkCreateAccelerationStructureKHR", vkCreateAccelerationStructureKHR(device_, &accelerationStructureCreateInfo, nullptr, &tlas_.accelerationStructure));
        }

        // The actual build process starts here

        // Create a scratch buffer as a temporary storage for the acceleration structure build
        Vulkan::Buffer scratchBuffer;
        scratchBuffer.Create(
            device_,
            physicalDeviceMemoryProperties_,
            accelerationStructureBuildSizesInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = {};
        accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationBuildGeometryInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.srcAccelerationStructure = update ? tlas_.accelerationStructure : VK_NULL_HANDLE;
        accelerationBuildGeometryInfo.dstAccelerationStructure = tlas_.accelerationStructure;
        accelerationBuildGeometryInfo.geometryCount = 1;
        accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
        accelerationBuildGeometryInfo.scratchData = scratchBuffer.GetBufferDeviceAddress();


        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo;
        accelerationStructureBuildRangeInfo.primitiveCount = static_cast<uint32_t>(meshInstancePool_.in_use_size());
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR* constAccelerationStructureBuildRangeInfo = &accelerationStructureBuildRangeInfo;
        
        // Build the acceleration structure on the device via a one-time command buffer submission
        // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
        VkCommandBuffer commandBuffer;
        CreateWorkerCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, graphicsCommandPool_, commandBuffer);
        vkCmdBuildAccelerationStructuresKHR(
            commandBuffer,
            1,
            &accelerationBuildGeometryInfo,
            &constAccelerationStructureBuildRangeInfo);
        SubmitWorkerCommandBuffer(commandBuffer, graphicsCommandPool_, graphicsQueue_);

        scratchBuffer.Destroy();

        // Get the top acceleration structure's handle, which will be used to setup it's descriptor
        VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo = {};
        accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationStructureDeviceAddressInfo.accelerationStructure = tlas_.accelerationStructure;
        tlas_.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device_, &accelerationStructureDeviceAddressInfo);

        PFG_EDITORLOG("Succesfully built tlas");
        
        // We did any pending work, reset flags
        rebuildTlas_= false;
        updateTlas_ = false;
    }

    void RayTracer::Prepare() 
    {
        if (alreadyPrepared_)
        {
            return;
        }

        CreateDescriptorSetsLayouts();
        CreateDescriptorPool();

        alreadyPrepared_ = true;
    }

    void RayTracer::ResetPipeline()
    {
        if (pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device_, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
    }

    void RayTracer::UpdateCamera(int cameraInstanceId, float* camPos, float* camDir, float* camUp, float* camSide, float* camNearFarFov)
    {
        if (renderTargets_.find(cameraInstanceId) == renderTargets_.end())
        {
            // The camera isn't in the system yet, don't attempt to update
            return;
        }

        auto& renderTarget = renderTargets_[cameraInstanceId];

        if (renderTarget->cameraData.GetBuffer() == VK_NULL_HANDLE)
        {
            renderTarget->cameraData.Create(
                device_,
                physicalDeviceMemoryProperties_,
                sizeof(ShaderCameraParam),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                Vulkan::Buffer::kDefaultMemoryPropertyFlags);
        }

        auto camera = reinterpret_cast<ShaderCameraParam*>(renderTarget->cameraData.Map());
        camera->camPos.x = camPos[0];
        camera->camPos.y = camPos[1];
        camera->camPos.z = camPos[2];

        camera->camDir.x = camDir[0];
        camera->camDir.y = camDir[1];
        camera->camDir.z = camDir[2];

        camera->camUp.x = camUp[0];
        camera->camUp.y = camUp[1];
        camera->camUp.z = camUp[2];

        camera->camSide.x = camSide[0];
        camera->camSide.y = camSide[1];
        camera->camSide.z = camSide[2];

        camera->camNearFarFov.x = camNearFarFov[0];
        camera->camNearFarFov.y = camNearFarFov[1];
        camera->camNearFarFov.z = camNearFarFov[2];

        renderTarget->cameraData.Unmap();
        
        //PFG_EDITORLOG("Updated camera " + std::to_string(cameraInstanceId));
    }

    void RayTracer::UpdateSceneData(float* color) 
    {
        if (sceneData_.GetBuffer() == VK_NULL_HANDLE)
        {
            sceneData_.Create(
                device_,
                physicalDeviceMemoryProperties_,
                sizeof(ShaderSceneParam),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                Vulkan::Buffer::kDefaultMemoryPropertyFlags);
        }
    
        auto scene = reinterpret_cast<ShaderSceneParam*>(sceneData_.Map());
        scene->ambient.r = color[0];
        scene->ambient.g = color[1];
        scene->ambient.b = color[2];
        scene->ambient.a = color[3];

        sceneData_.Unmap();
    }

    void RayTracer::TraceRays(int cameraInstanceId)
    {
        if (renderTargets_.find(cameraInstanceId) == renderTargets_.end())
        {
            // The camera isn't in the system yet, don't attempt to trace
            return;
        }

        if (renderTargets_[cameraInstanceId]->cameraData.GetBuffer() == VK_NULL_HANDLE)
        {
            // This camera hasn't been updated get for render
            return;
        }

        if (tlas_.accelerationStructure == VK_NULL_HANDLE)
        {
            PFG_EDITORLOG("We don't have a tlas, so we cannot trace rays!");
            return;
        }

        if (pipelineLayout_ == VK_NULL_HANDLE)
        {
            CreatePipelineLayout();
            if (pipelineLayout_ == VK_NULL_HANDLE)
            {
                PFG_EDITORLOG("Something went wrong with creating the pipeline layout");
                // Something went wrong, don't continue
                return;
            }
        }

        if (pipeline_ == VK_NULL_HANDLE)
        {
            CreatePipeline();
            if (pipeline_ == VK_NULL_HANDLE)
            {
                PFG_EDITORLOG("Something went wrong with creating the pipeline");
                return;
            }
        }
        
        if (pipeline_ != VK_NULL_HANDLE && pipelineLayout_!= VK_NULL_HANDLE)
        {
            BuildDescriptorBufferInfos(cameraInstanceId);
            UpdateDescriptorSets(cameraInstanceId);

            // cannot manage resources inside renderpass
            graphicsInterface_->EnsureOutsideRenderPass();

            UnityVulkanRecordingState recordingState;
            if (!graphicsInterface_->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
            {
                return;
            }

            BuildAndSubmitRayTracingCommandBuffer(cameraInstanceId, recordingState.commandBuffer);
            CopyRenderToRenderTarget(cameraInstanceId, recordingState.commandBuffer);
        }
    }

#pragma endregion RayTracerAPI

    void RayTracer::CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPool& outCommandPool)
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = {};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK("vkCreateCommandPool", vkCreateCommandPool(device_, &commandPoolCreateInfo, nullptr, &outCommandPool));
    }

    void RayTracer::CreateWorkerCommandBuffer(VkCommandBufferLevel level, VkCommandPool commandPool, VkCommandBuffer& outCommandBuffer)
    {
        //TODO : Can we just use a recording state from unity?
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.level = level;
        commandBufferAllocateInfo.commandBufferCount = 1;

        VK_CHECK("vkAllocateCommandBuffers", vkAllocateCommandBuffers(device_, &commandBufferAllocateInfo, &outCommandBuffer));
    
        // Start recording for the new command buffer
        VkCommandBufferBeginInfo commandBufferBeginInfo = { };
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK("vkBeginCommandBuffer", vkBeginCommandBuffer(outCommandBuffer, &commandBufferBeginInfo));
    }

    void RayTracer::SubmitWorkerCommandBuffer(VkCommandBuffer commandBuffer, VkCommandPool commandPool, const VkQueue& queue)
    {
        VK_CHECK("vkEndCommandBuffer", vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
    
        /*if (signalSemaphore)
        {
            submit_info.pSignalSemaphores = &signalSemaphore;
            submit_info.signalSemaphoreCount = 1;
        }*/
    
        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fenceCreateInfo = { };
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = 0;
    
        VkFence fence;
        VK_CHECK("vkCreateFence", vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence));
    
        // Submit to the queue
        VkResult result = vkQueueSubmit(queue, 1, &submitInfo, fence);
        if (result == VK_ERROR_DEVICE_LOST)
        {

        }

        VK_CHECK("vkQueueSubmit", result);
    
        // Wait for the fence to signal that command buffer has finished executing
        const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;
        VK_CHECK("vkWaitForFences", vkWaitForFences(device_, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
    
        vkDestroyFence(device_, fence, nullptr);

        if (commandPool != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
        }
    }

    void RayTracer::BuildBlas(int sharedMeshPoolIndex)
    {
        // Create buffers for the bottom level geometry
    
        // Setup a single transformation matrix that can be used to transform the whole geometry for a single bottom level acceleration structure
        VkTransformMatrixKHR transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f };
        
        Vulkan::Buffer transformBuffer;
        transformBuffer.Create(
            device_, 
            physicalDeviceMemoryProperties_, 
            sizeof(VkTransformMatrixKHR),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            Vulkan::Buffer::kDefaultMemoryPropertyFlags);
        transformBuffer.UploadData(&transformMatrix, sizeof(VkTransformMatrixKHR));

        //auto identity = reinterpret_cast<VkTransformMatrixKHR*>(transformBuffer.Map());
        //PFG_EDITORLOG("Identity matrix: ");
        //PFG_EDITORLOG(std::to_string(identity->matrix[0][0]) + ", " + std::to_string(identity->matrix[0][1]) + ", " + std::to_string(identity->matrix[0][2]) + ", " + std::to_string(identity->matrix[0][3]));
        //PFG_EDITORLOG(std::to_string(identity->matrix[1][0]) + ", " + std::to_string(identity->matrix[1][1]) + ", " + std::to_string(identity->matrix[1][2]) + ", " + std::to_string(identity->matrix[1][3]));
        //PFG_EDITORLOG(std::to_string(identity->matrix[2][0]) + ", " + std::to_string(identity->matrix[2][1]) + ", " + std::to_string(identity->matrix[2][2]) + ", " + std::to_string(identity->matrix[2][3]));
        //transformBuffer.Unmap();

        // The bottom level acceleration structure contains one set of triangles as the input geometry
        VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        accelerationStructureGeometry.geometry.triangles.pNext = nullptr;
        accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        accelerationStructureGeometry.geometry.triangles.vertexData = sharedMeshesPool_[sharedMeshPoolIndex]->vertexBuffer.GetBufferDeviceAddressConst();
        accelerationStructureGeometry.geometry.triangles.maxVertex = sharedMeshesPool_[sharedMeshPoolIndex]->vertexCount;
        accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(vec3);
        accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        accelerationStructureGeometry.geometry.triangles.indexData = sharedMeshesPool_[sharedMeshPoolIndex]->indexBuffer.GetBufferDeviceAddressConst();
        accelerationStructureGeometry.geometry.triangles.transformData = transformBuffer.GetBufferDeviceAddressConst();
        
        // Get the size requirements for buffers involved in the acceleration structure build process
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        // Number of triangles 
        const uint32_t primitiveCount = sharedMeshesPool_[sharedMeshPoolIndex]->indexCount / 3;
        /*PFG_EDITORLOG("BuildBlas() vertexCount: " + std::to_string(sharedMeshesPool_[sharedMeshPoolIndex]->vertexCount));
        PFG_EDITORLOG("BuildBlas() indexCount: " + std::to_string(sharedMeshesPool_[sharedMeshPoolIndex]->indexCount));*/

        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device_,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &primitiveCount,
            &accelerationStructureBuildSizesInfo);

        // Create a buffer to hold the acceleration structure
        sharedMeshesPool_[sharedMeshPoolIndex]->blas.buffer.Create(
            device_,
            physicalDeviceMemoryProperties_,
            accelerationStructureBuildSizesInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            Vulkan::Buffer::kDefaultMemoryPropertyFlags);
    
        // Create the acceleration structure
        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = sharedMeshesPool_[sharedMeshPoolIndex]->blas.buffer.GetBuffer();
        accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VK_CHECK("vkCreateAccelerationStructureKHR", vkCreateAccelerationStructureKHR(device_, &accelerationStructureCreateInfo, nullptr, &sharedMeshesPool_[sharedMeshPoolIndex]->blas.accelerationStructure));

        // The actual build process starts here
        // Create a scratch buffer as a temporary storage for the acceleration structure build
        Vulkan::Buffer scratchBuffer;
        scratchBuffer.Create(
            device_, 
            physicalDeviceMemoryProperties_, 
            accelerationStructureBuildSizesInfo.buildScratchSize, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = { };
        accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.dstAccelerationStructure = sharedMeshesPool_[sharedMeshPoolIndex]->blas.accelerationStructure;
        accelerationBuildGeometryInfo.geometryCount = 1;
        accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
        accelerationBuildGeometryInfo.scratchData = scratchBuffer.GetBufferDeviceAddress();

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = { };
        accelerationStructureBuildRangeInfo.primitiveCount = primitiveCount;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationStructureBuildRangeInfos = { &accelerationStructureBuildRangeInfo };

        // Build the acceleration structure on the device via a one-time command buffer submission.  We will NOT use the Unity command buffer in this case
        // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds

        VkCommandBuffer buildCommandBuffer;
        CreateWorkerCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, graphicsCommandPool_, buildCommandBuffer);
        vkCmdBuildAccelerationStructuresKHR(
            buildCommandBuffer,
            1,
            &accelerationBuildGeometryInfo,
            accelerationStructureBuildRangeInfos.data());
        SubmitWorkerCommandBuffer(buildCommandBuffer, graphicsCommandPool_, graphicsQueue_);

        transformBuffer.Destroy();
        scratchBuffer.Destroy();

        // Get the bottom acceleration structure's handle, which will be used during the top level acceleration build
        VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
        accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationStructureDeviceAddressInfo.accelerationStructure = sharedMeshesPool_[sharedMeshPoolIndex]->blas.accelerationStructure;
        sharedMeshesPool_[sharedMeshPoolIndex]->blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device_, &accelerationStructureDeviceAddressInfo);

        PFG_EDITORLOG("Built blas for mesh (sharedMeshInstanceId: " + std::to_string(sharedMeshesPool_[sharedMeshPoolIndex]->sharedMeshInstanceId) + ")");
    }

    void RayTracer::CreateDescriptorSetsLayouts()
    {
        // Create descriptor sets for the shader.  This setups up how data is bound to GPU memory and what shader stages will have access to what memory
    
        descriptorSetLayouts_.resize(DESCRIPTOR_SET_SIZE);

        // set 0:
        //  binding 0  ->  Acceleration structure
        //  binding 1  ->  Scene data
        //  binding 2  ->  Camera data
        {
            VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding;
            accelerationStructureLayoutBinding.binding = DESCRIPTOR_BINDING_ACCELERATION_STRUCTURE;
            accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            accelerationStructureLayoutBinding.descriptorCount = 1;
            accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            VkDescriptorSetLayoutBinding sceneDataLayoutBinding;
            sceneDataLayoutBinding.binding = DESCRIPTOR_BINDING_SCENE_DATA;
            sceneDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            sceneDataLayoutBinding.descriptorCount = 1;
            sceneDataLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            VkDescriptorSetLayoutBinding cameraDataLayoutBinding;
            cameraDataLayoutBinding.binding = DESCRIPTOR_BINDING_CAMERA_DATA;
            cameraDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            cameraDataLayoutBinding.descriptorCount = 1;
            cameraDataLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            std::vector<VkDescriptorSetLayoutBinding> bindings({
                    accelerationStructureLayoutBinding,
                    sceneDataLayoutBinding,
                    cameraDataLayoutBinding
                });

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            descriptorSetLayoutCreateInfo.pBindings = bindings.data();

            VK_CHECK("vkCreateDescriptorSetLayout", 
                vkCreateDescriptorSetLayout(device_, 
                                            &descriptorSetLayoutCreateInfo, 
                                            nullptr, 
                                            // Overkill, but represents the sets its creating                        
                                            &descriptorSetLayouts_[DESCRIPTOR_SET_ACCELERATION_STRUCTURE & DESCRIPTOR_SET_SCENE_DATA & DESCRIPTOR_SET_CAMERA_DATA]));
        }

        // Set 1
        // binding 0 -> render target 
        {
            VkDescriptorSetLayoutBinding imageLayoutBinding;
            imageLayoutBinding.binding = DESCRIPTOR_BINDING_RENDER_TARGET;
            imageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            imageLayoutBinding.descriptorCount = 1;
            imageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = 1;
            descriptorSetLayoutCreateInfo.pBindings = &imageLayoutBinding;
            
            VK_CHECK("vkCreateDescriptorSetLayout", vkCreateDescriptorSetLayout(device_, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts_[DESCRIPTOR_SET_RENDER_TARGET]));
        }

        // set 2
        // binding 0 -> attributes
        {
            const VkDescriptorBindingFlags setFlag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

            VkDescriptorSetLayoutBindingFlagsCreateInfo setBindingFlags;
            setBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            setBindingFlags.pNext = nullptr;
            setBindingFlags.pBindingFlags = &setFlag;
            setBindingFlags.bindingCount = 1;

            VkDescriptorSetLayoutBinding verticesLayoutBinding;
            verticesLayoutBinding.binding = DESCRIPTOR_BINDING_VERTEX_ATTRIBUTES;
            verticesLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            verticesLayoutBinding.descriptorCount = static_cast<uint32_t>(sharedMeshAttributesPool_.pool_size());
            verticesLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = 1;
            descriptorSetLayoutCreateInfo.pBindings = &verticesLayoutBinding;
            descriptorSetLayoutCreateInfo.pNext = &setBindingFlags;

            VK_CHECK("vkCreateDescriptorSetLayout", vkCreateDescriptorSetLayout(device_, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts_[DESCRIPTOR_SET_VERTEX_ATTRIBUTES]));
        }

        // set 3
        // binding 0 -> faces
        {
            const VkDescriptorBindingFlags setFlag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

            VkDescriptorSetLayoutBindingFlagsCreateInfo setBindingFlags;
            setBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            setBindingFlags.pNext = nullptr;
            setBindingFlags.pBindingFlags = &setFlag;
            setBindingFlags.bindingCount = 1;

            VkDescriptorSetLayoutBinding indicesLayoutBinding;
            indicesLayoutBinding.binding = DESCRIPTOR_BINDING_FACE_DATA;
            indicesLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            indicesLayoutBinding.descriptorCount = static_cast<uint32_t>(sharedMeshFacesPool_.pool_size());;
            indicesLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = 1;
            descriptorSetLayoutCreateInfo.pBindings = &indicesLayoutBinding;
            descriptorSetLayoutCreateInfo.pNext = &setBindingFlags;

            VK_CHECK("vkCreateDescriptorSetLayout", vkCreateDescriptorSetLayout(device_, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts_[DESCRIPTOR_SET_FACE_DATA]));
        }
    }

    void RayTracer::CreatePipelineLayout()
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { };
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = DESCRIPTOR_SET_SIZE;
        pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts_.data();

        VK_CHECK("vkCreatePipelineLayout", vkCreatePipelineLayout(device_, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout_));
    }

    void RayTracer::CreatePipeline()
    {
        Vulkan::Shader rayGenShader(device_);
        Vulkan::Shader rayChitShader(device_);
        Vulkan::Shader rayMissShader(device_);
        Vulkan::Shader shadowChit(device_);
        Vulkan::Shader shadowMiss(device_);

        rayGenShader.LoadFromFile((shaderFolder_ + "ray_gen.bin").c_str());
        rayChitShader.LoadFromFile((shaderFolder_ + "ray_chit.bin").c_str());
        rayMissShader.LoadFromFile((shaderFolder_ + "ray_miss.bin").c_str());
        shadowChit.LoadFromFile((shaderFolder_ + "shadow_ray_chit.bin").c_str());
        shadowMiss.LoadFromFile((shaderFolder_ + "shadow_ray_miss.bin").c_str());

        // Destroy any existing shader table before creating a new one
        shaderBindingTable_.Destroy();

        shaderBindingTable_.Initialize(2, 2, rayTracingProperties_.shaderGroupHandleSize, rayTracingProperties_.shaderGroupBaseAlignment);

        // Ray generation stage
        shaderBindingTable_.SetRaygenStage(rayGenShader.GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        // Hit stages
        shaderBindingTable_.AddStageToHitGroup({ rayChitShader.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, PRIMARY_HIT_SHADERS_INDEX);
        shaderBindingTable_.AddStageToHitGroup({ shadowChit.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, SHADOW_HIT_SHADERS_INDEX);

        // Define miss stages for both primary and shadow misses
        shaderBindingTable_.AddStageToMissGroup(rayMissShader.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR), PRIMARY_MISS_SHADERS_INDEX);
        shaderBindingTable_.AddStageToMissGroup(shadowMiss.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR), SHADOW_MISS_SHADERS_INDEX);

        // Create the pipeline for ray tracing based on shader binding table
        VkRayTracingPipelineCreateInfoKHR rayPipelineInfo = {};
        rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rayPipelineInfo.stageCount = shaderBindingTable_.GetNumStages();
        rayPipelineInfo.pStages = shaderBindingTable_.GetStages();
        rayPipelineInfo.groupCount = shaderBindingTable_.GetNumGroups();
        rayPipelineInfo.pGroups = shaderBindingTable_.GetGroups();
        rayPipelineInfo.maxPipelineRayRecursionDepth = 1;
        rayPipelineInfo.layout = pipelineLayout_;

        VK_CHECK("vkCreateRayTracingPipelinesKHR", vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayPipelineInfo, nullptr, &pipeline_));

        shaderBindingTable_.CreateSBT(device_, physicalDeviceMemoryProperties_, pipeline_);
    }

    void RayTracer::BuildAndSubmitRayTracingCommandBuffer(int cameraInstanceId, VkCommandBuffer commandBuffer)
    {
        // NOTE: assumes that renderTargets_ has already been checked

        UnityVulkanRecordingState recordingState;
        if (!graphicsInterface_->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
        {
            return;
        }

        auto& renderTarget = renderTargets_[cameraInstanceId];

        vkCmdBindPipeline(
            recordingState.commandBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            pipeline_);

        vkCmdBindDescriptorSets(
            recordingState.commandBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            pipelineLayout_, 0,
            static_cast<uint32_t>(renderTarget->descriptorSets.size()), renderTarget->descriptorSets.data(),
            0, 0);

        VkStridedDeviceAddressRegionKHR raygenShaderEntry = {};
        raygenShaderEntry.deviceAddress = shaderBindingTable_.GetBuffer().GetBufferDeviceAddress().deviceAddress + shaderBindingTable_.GetRaygenOffset();
        raygenShaderEntry.stride = shaderBindingTable_.GetGroupsStride();
        raygenShaderEntry.size = shaderBindingTable_.GetRaygenSize();

        VkStridedDeviceAddressRegionKHR missShaderEntry{};
        missShaderEntry.deviceAddress = shaderBindingTable_.GetBuffer().GetBufferDeviceAddress().deviceAddress + shaderBindingTable_.GetMissGroupsOffset();
        missShaderEntry.stride = shaderBindingTable_.GetGroupsStride();
        missShaderEntry.size = shaderBindingTable_.GetMissGroupsSize();

        VkStridedDeviceAddressRegionKHR hitShaderEntry{};
        hitShaderEntry.deviceAddress = shaderBindingTable_.GetBuffer().GetBufferDeviceAddress().deviceAddress + shaderBindingTable_.GetHitGroupsOffset();
        hitShaderEntry.stride = shaderBindingTable_.GetGroupsStride();
        hitShaderEntry.size = shaderBindingTable_.GetHitGroupsSize();

        VkStridedDeviceAddressRegionKHR callableShaderEntry{};

        // Dispatch the ray tracing commands
        vkCmdBindPipeline(recordingState.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_);
        vkCmdBindDescriptorSets(recordingState.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout_, 0, static_cast<uint32_t>(renderTarget->descriptorSets.size()), renderTarget->descriptorSets.data(), 0, 0);

        // Make into a storage image
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        Vulkan::Image::UpdateImageBarrier(
            recordingState.commandBuffer,
            renderTargets_[cameraInstanceId]->stagingImage.GetImage(),
            range,
            0, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        //PFG_EDITORLOG("Tracing for " + std::to_string(cameraInstanceId));

        vkCmdTraceRaysKHR(
            recordingState.commandBuffer,
            &raygenShaderEntry,
            &missShaderEntry,
            &hitShaderEntry,
            &callableShaderEntry,
            renderTargets_[cameraInstanceId]->extent.width,
            renderTargets_[cameraInstanceId]->extent.height,
            renderTargets_[cameraInstanceId]->extent.depth);

        //SubmitWorkerCommandBuffer(commandBuffer, graphicsCommandPool_, graphicsQueue_);
    }

    void RayTracer::CopyRenderToRenderTarget(int cameraInstanceId, VkCommandBuffer commandBuffer)
    {
        // NOTE: assumes that renderTargets_ has already been checked

        UnityVulkanImage image;
        if (!graphicsInterface_->AccessTexture(renderTargets_[cameraInstanceId]->destination,
            UnityVulkanWholeImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            kUnityVulkanResourceAccess_PipelineBarrier,
            &image))
        {
            return;
        }

        UnityVulkanRecordingState recordingState;
        if (!graphicsInterface_->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
        {
            return;
        }
        
        PFG_EDITORLOG("Copying render for " + std::to_string(cameraInstanceId));

        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageCopy region;
        region.extent.width = renderTargets_[cameraInstanceId]->extent.width;
        region.extent.height = renderTargets_[cameraInstanceId]->extent.height;
        region.extent.depth = 1;
        region.srcOffset.x = 0;
        region.srcOffset.y = 0;
        region.srcOffset.z = 0;
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.srcSubresource.mipLevel = 0;
        region.dstOffset.x = 0;
        region.dstOffset.y = 0;
        region.dstOffset.z = 0;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = 1;
        region.dstSubresource.mipLevel = 0;

        // Assign target image to be transfer optimal
        Vulkan::Image::UpdateImageBarrier(
            recordingState.commandBuffer,
            renderTargets_[cameraInstanceId]->stagingImage.GetImage(),
            range,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // BUG? Unity destination is not set to the correct layout, do it here
        Vulkan::Image::UpdateImageBarrier(
            recordingState.commandBuffer,
            image.image,
            range,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkCmdCopyImage(recordingState.commandBuffer, renderTargets_[cameraInstanceId]->stagingImage.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Revert target image 
        Vulkan::Image::UpdateImageBarrier(
            recordingState.commandBuffer,
            renderTargets_[cameraInstanceId]->stagingImage.GetImage(),
            range,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

        // BUG? Unity destination is not set to the correct layout, revert
        Vulkan::Image::UpdateImageBarrier(
            recordingState.commandBuffer,
            image.image,
            range,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void RayTracer::BuildDescriptorBufferInfos(int cameraInstanceId)
    {
        // NOTE: assumes that renderTargets_ has already been checked 

        auto& renderTarget = renderTargets_[cameraInstanceId];

        renderTarget->cameraDataBufferInfo.buffer = renderTarget->cameraData.GetBuffer();
        renderTarget->cameraDataBufferInfo.offset = 0;
        renderTarget->cameraDataBufferInfo.range = renderTarget->cameraData.GetSize();

        //PFG_EDITORLOG("Updated BuildDescriptorBufferInfos for " + std::to_string(cameraInstanceId));

        // TODO: move all below here because its unnecessary to do this each build?
        sceneBufferInfo_.buffer = sceneData_.GetBuffer();
        sceneBufferInfo_.offset = 0;
        sceneBufferInfo_.range = sceneData_.GetSize();
       
        sharedMeshAttributesBufferInfos_.clear();
        sharedMeshAttributesBufferInfos_.resize(sharedMeshAttributesPool_.pool_size());
        for (int i = 0; i < sharedMeshAttributesPool_.pool_size(); ++i)
        {
            VkDescriptorBufferInfo& bufferInfo = sharedMeshAttributesBufferInfos_[i];
            const Vulkan::Buffer& buffer = sharedMeshAttributesPool_[i];

            bufferInfo.buffer = buffer.GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = buffer.GetSize();

            sharedMeshAttributesBufferInfos_.push_back(bufferInfo);
        }

        sharedMeshFacesBufferInfos_.clear();
        sharedMeshFacesBufferInfos_.resize(sharedMeshFacesPool_.pool_size());
        for (int i = 0; i < sharedMeshFacesPool_.pool_size(); ++i)
        {
            VkDescriptorBufferInfo& bufferInfo = sharedMeshFacesBufferInfos_[i];
            const Vulkan::Buffer& buffer = sharedMeshFacesPool_[i];

            bufferInfo.buffer = buffer.GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = buffer.GetSize();

            sharedMeshFacesBufferInfos_.push_back(bufferInfo);
        }
    }
    
    void RayTracer::CreateDescriptorPool() 
    {   
        // Descriptors are not generated directly, but from a pool.  Create that pool here
        std::vector<VkDescriptorPoolSize> poolSizes({
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },       // Top level acceleration structure
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },                    // Game Render Target + Scene Render Target
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},                    // Scene data + Camera data
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 * 2 }             // vertex attribs for each mesh + faces buffer for each mesh  Supports 1000 meshes at once?
            });
    
        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.pNext = nullptr;
        descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // This allows vkFreeDescriptorSets to be called
        descriptorPoolCreateInfo.maxSets = 100;  // I have no idea what this should be T_T
        descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    
        VK_CHECK("vkCreateDescriptorPool", vkCreateDescriptorPool(device_, &descriptorPoolCreateInfo, nullptr, &descriptorPool_));

        PFG_EDITORLOG("Successfully created descriptor pool");
    }
    
    void RayTracer::UpdateDescriptorSets(int cameraInstanceId)
    {
        // NOTE: assumes that renderTargets_ has already been checked 
        auto& renderTarget = renderTargets_[cameraInstanceId];

        if (!renderTarget->updateDescriptorSetsData)
        {
            return;
        }

        if (renderTarget->descriptorSets.size() > 0)
        {
            // Free existing descriptor sets before attempting to allocate new ones!
            vkFreeDescriptorSets(device_, descriptorPool_, DESCRIPTOR_SET_SIZE, renderTarget->descriptorSets.data());
        }

        // Update the descriptor sets with the actual data to store in memory.
    
        // Now use the pool to upload data for each descriptor
        renderTarget->descriptorSets.resize(DESCRIPTOR_SET_SIZE);
        std::vector<uint32_t> variableDescriptorCounts({
            1,                                                              // Set 0
            1,                                                              // Set 1
            static_cast<uint32_t>(sharedMeshAttributesPool_.pool_size()),   // Set 2
            static_cast<uint32_t>(sharedMeshFacesPool_.pool_size())         // Set 3
            });
    
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo;
        variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variableDescriptorCountInfo.pNext = nullptr;
        variableDescriptorCountInfo.descriptorSetCount = DESCRIPTOR_SET_SIZE;
        variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data(); // actual number of descriptors
    
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool_;
        descriptorSetAllocateInfo.descriptorSetCount = DESCRIPTOR_SET_SIZE;
        descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts_.data();
    
        VK_CHECK("vkAllocateDescriptorSets", vkAllocateDescriptorSets(device_, &descriptorSetAllocateInfo, renderTarget->descriptorSets.data()));
    
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        // Set 0
        {
            // Acceleration Structure
            {
                VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
                descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                descriptorAccelerationStructureInfo.pNext = nullptr;
                descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
                descriptorAccelerationStructureInfo.pAccelerationStructures = &tlas_.accelerationStructure;

                VkWriteDescriptorSet accelerationStructureWrite;
                accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
                accelerationStructureWrite.dstSet = renderTarget->descriptorSets[DESCRIPTOR_SET_ACCELERATION_STRUCTURE];
                accelerationStructureWrite.dstBinding = DESCRIPTOR_BINDING_ACCELERATION_STRUCTURE;
                accelerationStructureWrite.dstArrayElement = 0;
                accelerationStructureWrite.descriptorCount = 1;
                accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                accelerationStructureWrite.pImageInfo = nullptr;
                accelerationStructureWrite.pBufferInfo = nullptr;
                accelerationStructureWrite.pTexelBufferView = nullptr;

                descriptorWrites.push_back(accelerationStructureWrite);
            }

            // Scene data
            {
                VkWriteDescriptorSet sceneBufferWrite;
                sceneBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                sceneBufferWrite.pNext = nullptr;
                sceneBufferWrite.dstSet = renderTarget->descriptorSets[DESCRIPTOR_SET_SCENE_DATA];
                sceneBufferWrite.dstBinding = DESCRIPTOR_BINDING_SCENE_DATA;
                sceneBufferWrite.dstArrayElement = 0;
                sceneBufferWrite.descriptorCount = 1;
                sceneBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                sceneBufferWrite.pImageInfo = nullptr;
                sceneBufferWrite.pBufferInfo = &sceneBufferInfo_;
                sceneBufferWrite.pTexelBufferView = nullptr;

                descriptorWrites.push_back(sceneBufferWrite);
            }

            // Camera data
            {
                VkWriteDescriptorSet camdataBufferWrite;
                camdataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                camdataBufferWrite.pNext = nullptr;
                camdataBufferWrite.dstSet = renderTarget->descriptorSets[DESCRIPTOR_SET_CAMERA_DATA];
                camdataBufferWrite.dstBinding = DESCRIPTOR_BINDING_CAMERA_DATA;
                camdataBufferWrite.dstArrayElement = 0;
                camdataBufferWrite.descriptorCount = 1;
                camdataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                camdataBufferWrite.pImageInfo = nullptr;
                camdataBufferWrite.pBufferInfo = &renderTargets_[cameraInstanceId]->cameraDataBufferInfo;
                camdataBufferWrite.pTexelBufferView = nullptr;

                descriptorWrites.push_back(camdataBufferWrite);
            }
        }

        // Set 1
        // Declared outside of scope so it isn't destroyed before write
        {
            // Render target
            {
                // From renderTargets_ map, Game = 1
                VkDescriptorImageInfo descriptorRenderTargetGameImageInfo;
                descriptorRenderTargetGameImageInfo.sampler = VK_NULL_HANDLE;
                descriptorRenderTargetGameImageInfo.imageView = renderTargets_[cameraInstanceId]->stagingImage.GetImageView();
                descriptorRenderTargetGameImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                VkWriteDescriptorSet renderTargetGameImageWrite;
                renderTargetGameImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                renderTargetGameImageWrite.pNext = nullptr;
                renderTargetGameImageWrite.dstSet = renderTarget->descriptorSets[DESCRIPTOR_SET_RENDER_TARGET];
                renderTargetGameImageWrite.dstBinding = DESCRIPTOR_BINDING_RENDER_TARGET;
                renderTargetGameImageWrite.dstArrayElement = 0;
                renderTargetGameImageWrite.descriptorCount = 1;
                renderTargetGameImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                renderTargetGameImageWrite.pImageInfo = &descriptorRenderTargetGameImageInfo;
                renderTargetGameImageWrite.pBufferInfo = nullptr;
                renderTargetGameImageWrite.pTexelBufferView = nullptr;

                descriptorWrites.push_back(renderTargetGameImageWrite);
            }
        }
       
        // Set 2
        {
            // Vertex attributes
            {
                VkWriteDescriptorSet attribsBufferWrite;
                attribsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                attribsBufferWrite.pNext = nullptr;
                attribsBufferWrite.dstSet = renderTarget->descriptorSets[DESCRIPTOR_SET_VERTEX_ATTRIBUTES];
                attribsBufferWrite.dstBinding = DESCRIPTOR_BINDING_VERTEX_ATTRIBUTES;
                attribsBufferWrite.dstArrayElement = 0;
                attribsBufferWrite.descriptorCount = static_cast<uint32_t>(sharedMeshAttributesPool_.pool_size());
                attribsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                attribsBufferWrite.pImageInfo = nullptr;
                attribsBufferWrite.pBufferInfo = sharedMeshAttributesBufferInfos_.data();
                attribsBufferWrite.pTexelBufferView = nullptr;

                descriptorWrites.push_back(attribsBufferWrite);
            }

            // Faces
            {
                VkWriteDescriptorSet facesBufferWrite;
                facesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                facesBufferWrite.pNext = nullptr;
                facesBufferWrite.dstSet = renderTarget->descriptorSets[DESCRIPTOR_SET_FACE_DATA];
                facesBufferWrite.dstBinding = DESCRIPTOR_BINDING_FACE_DATA;
                facesBufferWrite.dstArrayElement = 0;
                facesBufferWrite.descriptorCount = static_cast<uint32_t>(sharedMeshFacesPool_.pool_size());;
                facesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                facesBufferWrite.pImageInfo = nullptr;
                facesBufferWrite.pBufferInfo = sharedMeshFacesBufferInfos_.data();
                facesBufferWrite.pTexelBufferView = nullptr;

                descriptorWrites.push_back(facesBufferWrite);
            }
        }
    
        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);
   
        // Make sure unnecessary updates aren't made
        renderTarget->updateDescriptorSetsData = false;

        PFG_EDITORLOG("Successfully updated descriptor sets for " + std::to_string(cameraInstanceId));
    }

}
