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

    std::vector<const char*> deviceExtensions = {
            "VK_KHR_portability_subset",
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

    Json readJsonFile(const fs::path& jsonPath) {

        std::ifstream f(jsonPath);

        if (!f) {
            throw std::runtime_error("failed to open JSON file: " + jsonPath.string());
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
        createInfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = debugCallback,
                .pUserData = nullptr // Optional
        };
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

    int rateDeviceSuitability(VkPhysicalDevice& device, bool& isDiscrete) {

        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;

        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures{};
        atomicFloatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &atomicFloatFeatures;


        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);

        int score = 1;

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
        createSyncObjects();
    }

    Core::~Core() {

        vkDestroyDescriptorPool(device, storageDescriptorPool, nullptr);

        vkDestroyDevice(device, nullptr);

#ifdef ENABLE_VALIDATION_LAYER
        DestroyDebugUtilsMessengerEXT(instance, m_debugMessenger, nullptr);
#endif
        vkDestroyInstance(instance, nullptr);
    }

    void Core::createInstance() {

#ifdef ENABLE_VALIDATION_LAYER
        if (!checkValidationLayer()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }
#endif

        VkApplicationInfo appInfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = nullptr,
                .pApplicationName = "VkHydroCore",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "VKHydroCoreEngine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_2
        };

        auto extensions = getRequiredExtensions();

        VkInstanceCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
                .pApplicationInfo = &appInfo,
                .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                .ppEnabledExtensionNames = extensions.data(),
        };

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
            int score = rateDeviceSuitability(pDevice, isDiscrete);
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

            VkDeviceQueueCreateInfo queueCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = queueFamily,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority,
            };
            queueCreateInfos.emplace_back(queueCreateInfo);
        }

        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures{};
        atomicFloatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
        atomicFloatFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE;
        atomicFloatFeatures.shaderBufferFloat32Atomics = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &atomicFloatFeatures;

        VkDeviceCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = &deviceFeatures2,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos = queueCreateInfos.data(),
                .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                .ppEnabledExtensionNames = deviceExtensions.data(),
//                .pEnabledFeatures = &deviceFeatures
        };

#ifdef ENABLE_VALIDATION_LAYER
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
#else
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
#endif

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical m_device!");
        }
        vkGetDeviceQueue(device, indices.computeFamily.value(), 0, &computeQueue);
    }

    void Core::createCommandPool() {

        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queueFamilyIndices.computeFamily.value()
        };

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute command pool");
        }
    }

    void Core::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {

        VkCommandBufferAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
        };

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion = {
                .srcOffset = srcOffset,
                .dstOffset = dstOffset,
                .size = size,
        };
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer
        };

        vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(computeQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void Core::createCommandBuffer() {

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate compute command buffers!");
        }
        commandBuffers.emplace_back(commandBuffer);
    }

    void Core::createSyncObjects() {

        VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        VkFenceCreateInfo fenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeFinishedSemaphores) != VK_SUCCESS || vkCreateFence(device, &fenceInfo, nullptr, &computeInFlightFences) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute synchronization objects for a frame!");
        }
    }

    void Core::createFence() {

        VkFenceCreateInfo fenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

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

    void Core::createUniformBuffer(const std::string& name, Buffer*& uniformBuffer, Block& blockMemory) {

        auto stagingBuffer = createTempStagingBuffer(blockMemory.size);
        stagingBuffer.writeData(blockMemory.buffer.get());

        uniformBuffer = new Buffer(device, name, physicalDevice,
                                   blockMemory.size,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        copyBuffer(stagingBuffer.buffer, uniformBuffer->buffer, blockMemory.size);
    }

    void Core::createStorageBuffer(const std::string& name, Buffer*& storageBuffer, Block& blockMemory) {

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

        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = currentCommandBufferIndex,
                .pCommandBuffers = commandBuffers.data(),
        };

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
        VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
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

    void Core::parseScript(const char *path) {

        // Read json
        Json script = readJsonFile(path);

        // Get assets
        const auto& bufferLayouts = script["bufferLayouts"];
        const auto& dataResource = script["dataResource"];
        const auto& uniforms = script["uniforms"];
        const auto& storages = script["storages"];
        const auto& shaders = script["shaders"];
        const auto& passes = script["passes"];
        const auto& flow = script["flow"];

        // Create storages
        uint32_t bindingIndex = 0;
        for (const auto& storageInfo: storages) {
            Buffer* buffer = nullptr;
            std::string name = storageInfo["name"];
            Json layout = bufferLayouts[storageInfo["layout"]];
            std::vector<std::string> typeList = layout["type"];
            std::vector<std::string> dataNames = layout["data"];
            for (const auto& dataName: dataNames) {
                Block block(typeList, dataResource[dataName]);
                createStorageBuffer(name, buffer, block);
                name_buffer_map.emplace(name, std::shared_ptr<Buffer>(buffer));
            }
            buffer_descriptorSetPool_Map.emplace(name, std::array<uint32_t, 2>{ 0, bindingIndex++});
        }

        // Create uniforms
        bindingIndex = 0;
        for (const auto& uniformInfo: uniforms ) {
            Buffer* buffer = nullptr;
            std::string name = uniformInfo["name"];
            Json layout = bufferLayouts[uniformInfo["layout"]];
            std::vector<std::string> typeList = layout["type"];
            std::vector<std::string> dataNames = layout["data"];
            for (const auto& dataName: dataNames) {
                Block block(typeList, dataResource[dataName]);
                createUniformBuffer(name, buffer, block);
                name_buffer_map.emplace(name, std::shared_ptr<Buffer>(buffer));
            }
            buffer_descriptorSetPool_Map.emplace(name, std::array<uint32_t , 2>{ 1, bindingIndex++});
        }

        // Create descriptor pool
        uint32_t sizeFactor = 100;
        uint32_t storageBufferNum = storages.size();
        uint32_t uniformBufferNum = uniforms.size();
        std::vector<VkDescriptorPoolSize> poolSizes;
        if (storageBufferNum > 0) poolSizes.push_back({
                                                              .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              .descriptorCount = storageBufferNum * sizeFactor
                                                      });
        if (uniformBufferNum > 0) poolSizes.push_back({
                                                              .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                              .descriptorCount = uniformBufferNum * sizeFactor
                                                      });
        VkDescriptorPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 2 * sizeFactor,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data()
        };
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }

        // Create descriptor set layout pool
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts(2);
        std::vector<VkDescriptorSetLayoutBinding> storageBindings(storageBufferNum);
        std::vector<VkDescriptorSetLayoutBinding> uniformBindings(uniformBufferNum);
        for (size_t i = 0; i < storageBufferNum; ++i) {
            storageBindings[i].binding = i;
            storageBindings[i].descriptorCount = 1;
            storageBindings[i].pImmutableSamplers = nullptr;
            storageBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            storageBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        VkDescriptorSetLayoutCreateInfo storageLayoutInfo;
        storageLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        storageLayoutInfo.pBindings = storageBindings.data();
        storageLayoutInfo.bindingCount = storageBufferNum;
        storageLayoutInfo.pNext = nullptr;
        storageLayoutInfo.flags = 0;
        if (vkCreateDescriptorSetLayout(device, &storageLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create storage descriptor set layout!");
        }
        for (size_t i = 0; i < uniformBufferNum; ++i) {
            uniformBindings[i].binding = i;
            uniformBindings[i].descriptorCount = 1;
            uniformBindings[i].pImmutableSamplers = nullptr;
            uniformBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            uniformBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        VkDescriptorSetLayoutCreateInfo uniformLayoutInfo;
        uniformLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        uniformLayoutInfo.pBindings = uniformBindings.data();
        uniformLayoutInfo.bindingCount = uniformBufferNum;
        uniformLayoutInfo.pNext = nullptr;
        uniformLayoutInfo.flags = 0;
        if (vkCreateDescriptorSetLayout(device, &uniformLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create uniform descriptor set layout!");
        }

        // Allocate descriptor set in GPU
        VkDescriptorSetAllocateInfo allocInfo;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorSetCount = descriptorSetLayouts.size();
        allocInfo.pSetLayouts = descriptorSetLayouts.data();
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.pNext = nullptr;
        descriptorSetPool.resize(descriptorSetLayouts.size());
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSetPool.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor set for descriptor set pool!");
        }

        bindingIndex = 0;
        descriptorWriteSets.resize(storageBufferNum + uniformBufferNum);
        for (const auto& pair : buffer_descriptorSetPool_Map) {
            auto bufferName = pair.first;
            auto bindingInfo = pair.second;
            VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            switch (bindingInfo[1]) {
                case 0:
                    descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                case 1:
                    descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
            }
            descriptorWriteSets[bindingIndex++] = VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSetPool[bindingInfo[0]],
                    .dstBinding = static_cast<uint32_t>(bindingInfo[1]),
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = descriptorType,
                    .pBufferInfo = &name_buffer_map[bufferName].get()->getDescriptorBufferInfo(0, 0)
            };
        }

        // Create pipelines
        for (const auto& shaderInfo: shaders) {
            auto name = shaderInfo["name"].get<std::string>();
            auto bindingResource = shaderInfo["resource"];
            auto glslCode = readShaderFile(shaderInfo["path"].get<std::string>());
            const auto& pipeline = name_pipeline_map.emplace(name, std::make_shared<ComputePipeline>(device, name.c_str(), glslCode.c_str())).first->second;

            // Allocate descriptor sets for pipeline
            VkDescriptorSetAllocateInfo pipelineAllocInfo = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = descriptorPool,
                    .descriptorSetCount = static_cast<uint32_t>(pipeline->descriptorSetLayout.size()),
                    .pSetLayouts = pipeline->descriptorSetLayout.data()
            };
            if (vkAllocateDescriptorSets(device, &pipelineAllocInfo, pipeline->descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets for pipeline!");
            }

            // Make connection between <descriptorSetPool> of Core and <descriptorSets> of pipeline
            for (const auto& binding : bindingResource) {
                std::string bindingName = binding["name"];
                uint32_t bindingId = binding["binding"];
                uint32_t bindingSet = binding["set"];
                const auto& bindingInfo = buffer_descriptorSetPool_Map[bindingName];
                descriptorCopySets.emplace_back(VkCopyDescriptorSet{
                        .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
                        .srcSet = descriptorSetPool[bindingInfo[0]],
                        .srcBinding = bindingInfo[1],
                        .srcArrayElement = 0,
                        .dstSet = pipeline->descriptorSets[bindingSet],
                        .dstBinding = bindingId,
                        .dstArrayElement = 0,
                        .descriptorCount = 1
                });
            }
        }

        // Binding all resources to descriptor sets
        updateBindings();

        // Create passes
        for (const auto& passInfo: passes) {
            std::string name = passInfo["name"];
            std::string shader = passInfo["shader"];
            std::array<uint32_t , 3> groupCounts = passInfo["groupCounts"];
            name_pass_map.emplace(name, std::make_shared<ComputePass>(shader, groupCounts));
        }

        // Create flowNodes
        for (const auto& nodeInfo : flow) {
            std::vector<std::string> passNames = nodeInfo["passes"];
            std::vector<std::shared_ptr<ComputePass>> passPointers(passNames.size());
            for (size_t i = 0; i < passNames.size(); ++i) {
                passPointers[i] = name_pass_map[passNames[i]];
            }
            switch (nodeInfo["type"].get<size_t>()) {
                case 0b01:{
                    size_t count = nodeInfo["count"];
                    flowNode_list.emplace_back(std::make_unique<IterableFlowNode>(device, passPointers, count));
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

                    flowNode_list.emplace_back(std::make_unique<PollableFlowNode>(device, passPointers, flagBuffer, buffer, operation, flagIndex, flag, isDiscrete));
                    break;
                }
            }
        }
    }

    void Core::runScript() {

        Flag flag{ .f = 0.0 };
        const auto buffer = name_buffer_map["scalarBuffer"];
        for (const auto& node : flowNode_list) {

            while(node->isComplete()) {
                preheat();
                auto commandBuffer = commandBegin();

                for (const auto& pass: node->passes) {
                    Core::dispatch(commandBuffer, name_pipeline_map[pass->shader].get(), pass->groupCounts);
                }
                node->postProcess(commandBuffer);

                commandEnd();
                submit();

                buffer->readFlag(flag, 0);
                std::cout << "Dt: " << float(flag.u) / 10000.0 << std::endl;
            }
        }

        // Wait for end
        idle();
    }
}