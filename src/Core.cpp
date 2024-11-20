//
// Created by Yucheng Soku on 2024/10/30.
//
#include <set>
#include <map>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include "config.h"
#include "HydroCore/Core.h"
#include "nlohmann/json.hpp"

using Json = nlohmann::json;
namespace fs = std::filesystem;
namespace NextHydro {

    // ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Helpers ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> computeFamily;

        bool isComplete()
        {
            // Only compute family is necessary
            return computeFamily.has_value();
        }
    };

    std::vector<const char*> deviceExtensions = {
#ifdef PLATFORM_NEED_PORTABILITY
            "VK_KHR_portability_subset",
#endif
            "VK_EXT_shader_atomic_float"
    };

    std::vector<char> readFile(const char* filename) {

        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + std::string(filename));
        }

        std::streamsize fileSize = file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    Json readJsonFile(const std::string& jsonPath) {

        std::ifstream f(jsonPath);

        if (!f) {
            throw std::runtime_error("failed to open JSON file: " + jsonPath);
        }

        return Json::parse(f);
    }

    std::string readShaderFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filePath << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        return buffer.str();
    }

#ifdef ENABLE_VALIDATION_LAYER

    const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    void DestroyDebugUtilsMessengerEXT(
            VkInstance                          instance,
            VkDebugUtilsMessengerEXT            debugMessenger,
            const VkAllocationCallbacks*        pAllocator
    ) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, debugMessenger, pAllocator);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT          messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT                 messageType,
            const VkDebugUtilsMessengerCallbackDataEXT*     pCallbackData,
            void*                                           pUserData
    ) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    bool checkValidationLayer() {

        uint32_t  layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (auto layerName: validationLayers) {
            bool layerFound = false;
            for (auto layerProperties: availableLayers) {
                if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) {
                return false;
            }
        }
        return true;
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr; // Optional
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
    }

#endif

    std::vector<const char*> getRequiredExtensions() {

        auto extensions = std::vector<const char*>();
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
//        extensions.push_back(VK_EXT_SHADER_ATOMIC_FLOAT_2_EXTENSION_NAME);

#ifdef ENABLE_VALIDATION_LAYER
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        return extensions;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice& physicalDevice) {

        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension: availableExtensions) {

            // Vulkan requires physical devices to support "VK_KHR_portability_subset" if it exists
            if (extension.extensionName == std::string("VK_KHR_portability_subset")) {
                deviceExtensions.emplace_back("VK_KHR_portability_subset");
            }
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice& device) {

        QueueFamilyIndices indices{};

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily: queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                indices.computeFamily = i;
            }

            // HydroCore is only used for computation, presentSupport is not necessary!
//            VkBool32  presentSupport = false;
//            if (presentSupport) {
//                indices.presentFamily = i;
//            }

            if (indices.isComplete()) {
                break;
            }
            ++i;
        }
        return indices;
    }

    int rateDeviceSuitability(VkPhysicalDevice& device, uint32_t& maxComputeWorkGroupInvocations, bool& isDiscrete) {

        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;

        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        int score = 1;
        maxComputeWorkGroupInvocations = deviceProperties.limits.maxComputeWorkGroupInvocations;
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            isDiscrete = true;
            score += 1000;
        }
        if (deviceFeatures.shaderFloat64) {
            score += 1000;
        }
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        QueueFamilyIndices indices = findQueueFamilies(device);

        if (!indices.isComplete() || !extensionsSupported) {
            return 0;
        }

        return score;
    }

    VkResult CreateDebugUtilsMessengerEXT(
            VkInstance                    instance,
            const                         VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const                         VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT*     pDebugMessenger
    ) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        else
            return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    // /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Core ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Core::Core() {
        createInstance();
        setupDebugMessenger();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
    }

    Core::~Core() {

        // Destruct pipelines
        for (const auto& pipeline : name_pipeline_map) {
            auto module = pipeline.second->computeShaderModule->module;
            if (module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device, module, nullptr);
            }

            for (const auto& layout : pipeline.second->descriptorSetLayout) {
                if (layout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(device, layout, nullptr);
                }
            }

            if (pipeline.second->pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, pipeline.second->pipelineLayout, nullptr);
            }

            if (pipeline.second->pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, pipeline.second->pipeline, nullptr);
            }
        }

        // Destruct buffers
        for (const auto& buffer : name_buffer_map) {

            vkDestroyBuffer(device, buffer.second->buffer, nullptr);
            vkFreeMemory(device, buffer.second->memory, nullptr);
        }

        // Destruct fences
        for (auto& fence : fences) {
            vkDestroyFence(device, fence, nullptr);
        }

        // Destruct command buffer
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

        // Destruct descriptor set
        vkFreeDescriptorSets(device, descriptorPool, static_cast<uint32_t>(descriptorSetPool.size()), descriptorSetPool.data());

        // Destruct descriptor pool
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        // Destruct logical device
        vkDestroyDevice(device, nullptr);

#ifdef ENABLE_VALIDATION_LAYER
        DestroyDebugUtilsMessengerEXT(instance, m_debugMessenger, nullptr);
#endif

        // Destruct instance
        vkDestroyInstance(instance, nullptr);
    }

    void Core::createInstance() {

#ifdef ENABLE_VALIDATION_LAYER
        if (!checkValidationLayer()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }
#endif

        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "VKHydroCoreEngine";
        appInfo.pApplicationName = "VkHydroCore";
        appInfo.apiVersion = VK_API_VERSION_1_3;

        auto extensions = getRequiredExtensions();

        VkInstanceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef ENABLE_VALIDATION_LAYER
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
#else
        createInfo.enabledLayerCount = 0;
#endif

        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create Vulkan instance");
        }
    }

    void Core::setupDebugMessenger() {

#ifdef ENABLE_VALIDATION_LAYER
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
            throw std::runtime_error("failed to set up debug messenger!");
#endif
    }

    void Core::pickPhysicalDevice() {

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        std::multimap<int, VkPhysicalDevice> candidates;
        for (auto pDevice: physicalDevices) {
            int score = rateDeviceSuitability(pDevice, maxComputeWorkGroupInvocations, isDiscrete);
            candidates.insert(std::make_pair(score, pDevice));
        }

        if (candidates.rbegin()->first > 0) {
            physicalDevice = candidates.rbegin()->second;
        } else {
            throw std::runtime_error("failed to find suitable GPU!");
        }
    }

    void Core::createLogicalDevice() {

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.computeFamily.value() };

        float queuePriority = 1.0f;
        for (const auto& queueFamily : uniqueQueueFamilies) {

            VkDeviceQueueCreateInfo queueCreateInfo {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;

            queueCreateInfos.emplace_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        memset(&deviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));

        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures {};
        atomicFloatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
        atomicFloatFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE;
        atomicFloatFeatures.shaderBufferFloat32Atomics = VK_TRUE;

        VkPhysicalDeviceFeatures2 supportedFeatures2 {};
        supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT supportedAtomicFloatFeatures {};
        supportedAtomicFloatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
        supportedFeatures2.pNext = &supportedAtomicFloatFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures2);
        if (!supportedAtomicFloatFeatures.shaderBufferFloat32AtomicAdd ||
            !supportedAtomicFloatFeatures.shaderBufferFloat32Atomics) {
            throw std::runtime_error("atomic float is not supported on this device.");
        }

        VkDeviceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pNext = &atomicFloatFeatures;

#ifdef ENABLE_VALIDATION_LAYER
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
#else
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.enabledLayerCount = 0;
#endif

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical m_device!");
        }
        vkGetDeviceQueue(device, indices.computeFamily.value(), 0, &computeQueue);
    }

    void Core::createCommandPool() {

        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute command pool");
        }
    }

    void Core::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) const {

        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion;
        copyRegion.size = size;
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.commandBufferCount = 1;

        vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(computeQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void Core::createCommandBuffer() {

        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate compute command buffers!");
        }
        commandBuffers.emplace_back(commandBuffer);
    }

    void Core::createFence() {

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFence fence;
        if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create fence object!");
        }
        fences.emplace_back(fence);
    }

    void Core::idle() const {
        vkDeviceWaitIdle(device);
    }

    Buffer Core::createTempStagingBuffer(VkDeviceSize size) const {

        Buffer stagingBuffer(device, "", physicalDevice,
                             size,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        return stagingBuffer;
    }

    void Core::createStagingBuffer(const std::string& name, Buffer*& uniformBuffer, VkDeviceSize size) const {

        uniformBuffer = new Buffer(device, name, physicalDevice,
                                   size,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
    }

    void Core::createUniformBuffer(const std::string& name, Buffer*& uniformBuffer, Block& blockMemory) const {

        auto stagingBuffer = createTempStagingBuffer(blockMemory.size);
        stagingBuffer.writeData(blockMemory.buffer.get());

        uniformBuffer = new Buffer(device, name, physicalDevice,
                                   blockMemory.size,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        copyBuffer(stagingBuffer.buffer, uniformBuffer->buffer, blockMemory.size);
    }

    void Core::createStorageBuffer(const std::string& name, Buffer*& storageBuffer, Block& blockMemory) const {

        auto stagingBuffer = createTempStagingBuffer(blockMemory.size);
        stagingBuffer.writeData(blockMemory.buffer.get());

        storageBuffer = new Buffer(device, name, physicalDevice,
                                   blockMemory.size,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        copyBuffer(stagingBuffer.buffer, storageBuffer->buffer, blockMemory.size);
    }

    void Core::preheat() {
        if (currentFenceIndex == fences.size()) createFence();
        const auto& fence = fences[currentFenceIndex];

        vkResetFences(device, 1, &fence);
    }

    void Core::submit() {
        const auto& fence = fences[currentFenceIndex++];

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pCommandBuffers = commandBuffers.data();
        submitInfo.commandBufferCount = currentCommandBufferIndex;

        if (vkQueueSubmit(computeQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit compute command buffer!");
        }

        currentCommandBufferIndex = 0;
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        currentFenceIndex = 0;
    }

    VkCommandBuffer Core::commandBegin() {
        if (currentCommandBufferIndex == commandBuffers.size()) createCommandBuffer();

        auto commandBuffer = commandBuffers[currentCommandBufferIndex];
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording compute command buffer!");
        }
        return commandBuffer;
    }

    void Core::dispatch(const VkCommandBuffer& commandBuffer, const ComputePipeline* pipeline, const std::array<uint32_t, 3> groupCounts) {

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
        vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline->pipelineLayout,
                0,
                pipeline->descriptorSets.size(),
                pipeline->descriptorSets.data(),
                0,
                nullptr
        );
        vkCmdDispatch(commandBuffer, groupCounts[0], groupCounts[1], groupCounts[2]);
    }

    void Core::commandEnd() {

        if (vkEndCommandBuffer(commandBuffers[currentCommandBufferIndex++]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record compute command buffer!");
        }
    }

    void Core::updateBindings() const {
        vkUpdateDescriptorSets(device, descriptorWriteSets.size(), descriptorWriteSets.data(), descriptorCopySets.size(), descriptorCopySets.data());
    }

    void Core::parseScript(const std::string& path) {

        // Read json
        Json script = readJsonFile(path);

        // Get assets
        const auto& pipelines = script["pipelines"];
        const auto& storages = script["storages"];
        const auto& uniforms = script["uniforms"];
        const auto& passes = script["passes"];
        const auto& flow = script["flow"];

        // Create storages
        uint32_t bindingIndex = 0;
        for (const auto& storageInfo: storages) {
            Buffer* buffer = nullptr;
            std::string name = storageInfo["name"];
            const Json& layout = storageInfo["layout"];
            const Json& resource = storageInfo["resource"];
            Block block(layout, resource);
            createStorageBuffer(name, buffer, block);
            name_buffer_map.emplace(name, std::shared_ptr<Buffer>(buffer));
            buffer_descriptorSetPool_map.emplace(name, std::array<uint32_t, 2>{ 0, bindingIndex++});
        }

        // Create uniforms
        bindingIndex = 0;
        for (const auto& uniformInfo: uniforms ) {
            Buffer* buffer = nullptr;
            std::string name = uniformInfo["name"];
            const Json& layout = uniformInfo["layout"];
            const Json& resource = uniformInfo["resource"];
            Block block(layout, resource);
            createUniformBuffer(name, buffer, block);
            name_buffer_map.emplace(name, std::shared_ptr<Buffer>(buffer));
            buffer_descriptorSetPool_map.emplace(name, std::array<uint32_t , 2>{ 1, bindingIndex++});
        }

        // Create descriptor pool
        uint32_t sizeFactor = 100;
        uint32_t storageBufferNum = storages.size();
        uint32_t uniformBufferNum = uniforms.size();
        std::vector<VkDescriptorPoolSize> poolSizes;
        if (storageBufferNum > 0) poolSizes.push_back({
                                                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              storageBufferNum * sizeFactor
                                                      });
        if (uniformBufferNum > 0) poolSizes.push_back({
                                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                              uniformBufferNum * sizeFactor
                                                      });
        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 2 * sizeFactor;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }

        // Create descriptor set layout pool
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts(2);
        std::vector<VkDescriptorSetLayoutBinding> storageBindings(storageBufferNum);
        std::vector<VkDescriptorSetLayoutBinding> uniformBindings(uniformBufferNum);

        // - Storage buffer binding
        for (size_t i = 0; i < storageBufferNum; ++i) {
            storageBindings[i].binding = i;
            storageBindings[i].descriptorCount = 1;
            storageBindings[i].pImmutableSamplers = nullptr;
            storageBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            storageBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        VkDescriptorSetLayoutCreateInfo storageLayoutInfo {};
        storageLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        storageLayoutInfo.pBindings = storageBindings.data();
        storageLayoutInfo.bindingCount = storageBufferNum;
        if (vkCreateDescriptorSetLayout(device, &storageLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create storage descriptor set layout!");
        }

        // - Uniform buffer binding
        for (size_t i = 0; i < uniformBufferNum; ++i) {
            uniformBindings[i].binding = i;
            uniformBindings[i].descriptorCount = 1;
            uniformBindings[i].pImmutableSamplers = nullptr;
            uniformBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            uniformBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        VkDescriptorSetLayoutCreateInfo uniformLayoutInfo {};
        uniformLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        uniformLayoutInfo.pBindings = uniformBindings.data();
        uniformLayoutInfo.bindingCount = uniformBufferNum;
        if (vkCreateDescriptorSetLayout(device, &uniformLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create uniform descriptor set layout!");
        }

        // Allocate descriptor set in GPU
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorSetCount = descriptorSetLayouts.size();
        allocInfo.pSetLayouts = descriptorSetLayouts.data();
        allocInfo.descriptorPool = descriptorPool;
        descriptorSetPool.resize(descriptorSetLayouts.size());
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSetPool.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor set for descriptor set pool!");
        }

        bindingIndex = 0;
        descriptorWriteSets.resize(storageBufferNum + uniformBufferNum);
        for (const auto& pair : buffer_descriptorSetPool_map) {
            auto bufferName = pair.first;
            auto bindingInfo = pair.second;
            VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            switch (bindingInfo[0]) {
                case 0:
                    descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                case 1:
                    descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
            }

            descriptorWriteSets[bindingIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWriteSets[bindingIndex].dstSet = descriptorSetPool[bindingInfo[0]];
            descriptorWriteSets[bindingIndex].dstBinding = static_cast<uint32_t>(bindingInfo[1]);
            descriptorWriteSets[bindingIndex].dstArrayElement = 0;
            descriptorWriteSets[bindingIndex].descriptorCount = 1;
            descriptorWriteSets[bindingIndex].descriptorType = descriptorType;
            descriptorWriteSets[bindingIndex].pBufferInfo = &name_buffer_map[bufferName].get()->getDescriptorBufferInfo(0, 0);
            bindingIndex++;
        }

        // Create pipelines
        for (const auto& pipelineInfo: pipelines) {
            auto name = pipelineInfo["name"].get<std::string>();
            auto glslCode = readShaderFile(pipelineInfo["path"].get<std::string>());
            const auto& pipeline = name_pipeline_map.emplace(name, std::make_shared<ComputePipeline>(device, name.c_str(), glslCode.c_str())).first->second;

            // Allocate descriptor sets for pipeline
            VkDescriptorSetAllocateInfo pipelineAllocInfo {};
            pipelineAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            pipelineAllocInfo.descriptorSetCount = static_cast<uint32_t>(pipeline->descriptorSetLayout.size());
            pipelineAllocInfo.pSetLayouts = pipeline->descriptorSetLayout.data();
            pipelineAllocInfo.descriptorPool = descriptorPool;

            if (vkAllocateDescriptorSets(device, &pipelineAllocInfo, pipeline->descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets for pipeline!");
            }

            // Make connection between <descriptorSetPool> of Core and <descriptorSets> of pipeline
            for (size_t i = 0; i < pipeline->bindingResourceNames.size(); ++i) {
                auto bindingName = pipeline->bindingResourceNames[i];
                auto bindingId = pipeline->bindingResourceInfo[i][1];
                auto bindingSet = pipeline->bindingResourceInfo[i][0];
                const auto& bindingInfo = buffer_descriptorSetPool_map[bindingName];

                VkCopyDescriptorSet copyDescriptorSet {};
                copyDescriptorSet.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
                copyDescriptorSet.srcSet = descriptorSetPool[bindingInfo[0]];
                copyDescriptorSet.srcBinding = bindingInfo[1];
                copyDescriptorSet.srcArrayElement = 0;
                copyDescriptorSet.dstSet = pipeline->descriptorSets[bindingSet];
                copyDescriptorSet.dstBinding = bindingId;
                copyDescriptorSet.dstArrayElement = 0;
                copyDescriptorSet.descriptorCount = 1;
                descriptorCopySets.emplace_back(copyDescriptorSet);
            }
        }

        // Binding all resources to descriptor sets
        updateBindings();

        // Create passes
        for (const auto& passInfo: passes) {
            std::string name = passInfo["name"];
            std::string shader = passInfo["shader"];
            std::array<uint32_t , 3> computeScale = passInfo["computeScale"];
            auto groupWidth = sqrt(static_cast<double>(maxComputeWorkGroupInvocations));
            auto groupHeight = groupWidth;
            double groupDepth = 1.0;
            auto x = static_cast<uint32_t>(ceil(computeScale[0] / groupWidth));
            auto y = static_cast<uint32_t>(ceil(computeScale[1] / groupHeight));
            auto z = static_cast<uint32_t>(ceil(computeScale[2] / groupDepth));
            std::array<uint32_t , 3> groupCounts = { x, y, z };

            name_pass_map.emplace(name, std::make_shared<ComputePass>(shader, groupCounts));
        }

        // Create flowNodes
        for (const auto& nodeInfo : flow) {
            std::string nodeName = nodeInfo["nodeName"];
            std::vector<std::string> passNames = nodeInfo["passes"];
            std::vector<std::shared_ptr<ComputePass>> passPointers(passNames.size());
            for (size_t i = 0; i < passNames.size(); ++i) {
                passPointers[i] = name_pass_map[passNames[i]];
            }
            switch (nodeInfo["type"].get<size_t>()) {
                case 0b01:{
                    size_t count = nodeInfo["count"];
                    flowNode_list.emplace_back(std::make_unique<IterableCommandNode>(nodeName, device, passPointers, count));
                    break;
                }
                case 0b11: {
                    std::shared_ptr<Buffer> flagBuffer = name_buffer_map[nodeInfo["flagBuffer"]];
                    std::string operation = nodeInfo["operation"];
                    size_t flagIndex = nodeInfo["flagIndex"];
                    float_t flag = nodeInfo["flag"];

                    Buffer* buffer = nullptr;
                    std::string bufferName = "Flag Staging Buffer for " + nodeInfo["flagBuffer"].get<std::string>();
                    if (isDiscrete) {
                        createStagingBuffer(bufferName, buffer, 4);
                        name_buffer_map.emplace(bufferName, std::shared_ptr<Buffer>(buffer));
                    }

                    flowNode_list.emplace_back(std::make_unique<PollableCommandNode>(nodeName, device, passPointers, flagBuffer, buffer, operation, flagIndex, flag, isDiscrete));
                    break;
                }
            }
        }
    }

    void Core::runScript() {

        Flag flag {};
        const auto buffer = name_buffer_map["scalars"];
        for (const auto& node : flowNode_list) {

            node->tick();

            while(!node->isComplete()) {

                executeNode(node.get());

                buffer->readFlag(flag, 0);
                std::cout << "Dt: " << float(flag.u) / 10000.0 << std::endl;
            }
        }

        // Wait for end
        idle();
    }

    void Core::executeNode(ICommandNode* node) {

        preheat();
        auto commandBuffer = commandBegin();

        for (const auto& pass: node->passes) {
            Core::dispatch(commandBuffer, name_pipeline_map[pass->shader].get(), pass->groupCounts);
        }
        node->postProcess(commandBuffer);

        commandEnd();
        submit();
    }

    void Core::initialization(const std::string& path) {

        // Parse script first
        parseScript(path);

        // Find，run and remove initialization node
        // Command Node<__INIT__> can be non-unique, but must be ordered
        std::string nodeName = "__INIT__";
        auto deleteBegin = std::remove_if(
                flowNode_list.begin(),
                flowNode_list.end(),
                [this, &nodeName](const std::unique_ptr<ICommandNode>& node) -> bool {
                    if (node->name == nodeName) {
                        executeNode(node.get());
                        return true;
                    }
                    return false;
                }
        );
        flowNode_list.erase(deleteBegin, flowNode_list.end());
    }

    bool Core::step() {

        // Run Command Node<__STEP__>
        Flag flag {};
        const auto buffer = name_buffer_map["scalars"];
        for (const auto& node : flowNode_list) {
            executeNode(node.get());
        }

        buffer->readFlag(flag, 0);
        std::cout << "Dt: " << float(flag.u) / 10000.0 << std::endl;

        // Remove node if it is completed
        flowNode_list.erase(
                std::remove_if(
                        flowNode_list.begin(),
                        flowNode_list.end(),
                        [](const auto& node) -> bool {
                            return node->isComplete();
                        }
                ),
                flowNode_list.end()
        );

        // Return false if no node exists
        return !flowNode_list.empty();
    }
}