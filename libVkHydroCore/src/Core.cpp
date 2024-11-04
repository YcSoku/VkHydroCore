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

    int rateDeviceSuitability(VkPhysicalDevice& device) {

        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;

        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        int score = 1;

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }
        if (!deviceFeatures.shaderFloat64) {
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
        createComputeCommandBuffer();
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
                .apiVersion = VK_API_VERSION_1_3
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
            int score = rateDeviceSuitability(pDevice);
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

        VkPhysicalDeviceFeatures deviceFeatures {};
        VkDeviceCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos = queueCreateInfos.data(),
                .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                .ppEnabledExtensionNames = deviceExtensions.data(),
                .pEnabledFeatures = &deviceFeatures,
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

    Buffer Core::createStagingBuffer(VkDeviceSize size) const {

        Buffer stagingBuffer(device, "", physicalDevice,
                             size,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        return stagingBuffer;
    }

    ComputePipeline* Core::createComputePipeline(const char *shaderPath) const {

        return new ComputePipeline{ device, "", shaderPath };
    }

    void Core::createDescriptorPool() {

        VkDescriptorPoolSize poolSize = {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 2
        };

        VkDescriptorPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize,
        };

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &storageDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void Core::createComputeCommandBuffer() {

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &computeCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate compute command buffers!");
        }
    }

    void Core::recordComputeCommandBuffer(const ComputePipeline& computePipeline) const {

        VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        if (vkBeginCommandBuffer(computeCommandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording compute command buffer!");
        }

        vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline);
        vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipelineLayout, 0, computePipeline.descriptorSets.size(), computePipeline.descriptorSets.data(), 0,nullptr);

        vkCmdDispatch(computeCommandBuffer, 1, 1, 1);

        if (vkEndCommandBuffer(computeCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record compute command buffer!");
        }
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

    void Core::tick(ComputePipeline* computePipeline) {

        vkWaitForFences(device, 1, &computeInFlightFences, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &computeInFlightFences);

        vkResetCommandBuffer(computeCommandBuffer, 0);
        recordComputeCommandBuffer(*computePipeline);

        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &computeCommandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &computeFinishedSemaphores
        };

        if (vkQueueSubmit(computeQueue, 1, &submitInfo, computeInFlightFences) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit compute command buffer!");
        }
    }

    void Core::idle() const {
        vkDeviceWaitIdle(device);
    }

    void Core::parseScript(const char *path) {

        // Read json
        Json script = readJsonFile(path);

        // Get assets
        const auto& shaders = script["shaders"];
        const auto& buffers = script["buffers"];
        const auto& passes = script["passes"];

        // Create pipelines
        for (const auto& shaderInfo: shaders) {
            auto name = shaderInfo["name"].get<std::string>();
            auto glslCode = readShaderFile(shaderInfo["path"].get<std::string>());
            pipelineMap.emplace(name, std::make_unique<ComputePipeline>(device, name.c_str(), glslCode.c_str()));
        }

        // Create buffers
        for (const auto& bufferInfo: buffers) {
            std::string name = bufferInfo["name"];
            std::vector<float> data = bufferInfo["data"];
            auto buffer = createStorageBuffer(name.c_str(), data);
            bufferMap.emplace(bufferInfo["name"], std::unique_ptr<Buffer>(buffer));
        }

        // Create passes
        for (auto passInfo: passes) {
            std::vector<ResourceBinding> resource;
            std::string shader = passInfo["shader"];
            std::array<VkDeviceSize, 3> dispatch = passInfo["dispatch"];
            std::transform(passInfo["resource"].begin(), passInfo["resource"].end(), std::back_inserter(resource), &ResourceBinding::deserialize);
            passList.emplace_back(shader, resource, dispatch);
        }
    }

    void Core::runScript() {

        // Tick logic
        for (const auto& pass: passList) {
            auto pipeline = pipelineMap[pass.shader].get();

            for (const auto& buffer: pass.resource) {
                pipeline->bindBuffer(buffer.set, buffer.binding, bufferMap[buffer.name].get());
            }
            pipeline->tick();
        }

        // Tick compute
        for (const auto& pass: passList) {
            auto pipeline = pipelineMap[pass.shader].get();

            tick(pipeline);
        }

        // Wait for end
        idle();
    }
}