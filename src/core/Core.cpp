//
// Created by Yucheng Soku on 2024/10/30.
//
#include <set>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <vulkan/vulkan.h>
#include "config.h"
#include "core/Core.h"

namespace fs = std::filesystem;
namespace NextHydro {

    // ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Helpers ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::vector<const char*> deviceExtensions = {
    };

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
//    if (!deviceFeatures.shaderFloat64) {
//        return 0;
//    }
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
        createInfo.ppEnabledExtensionNames = nullptr;
#endif

        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
    }

    void Core::setupDebugMessenger() {

#ifndef ENABLE_VALIDATION_LAYER
        return;
#endif

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
            throw std::runtime_error("failed to set up debug messenger!");
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
        for (auto queueFamily : uniqueQueueFamilies) {

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

        Buffer stagingBuffer(device, physicalDevice,
                             size,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        return stagingBuffer;
    }

    VkShaderModule Core::createShaderModule(const std::vector<char>& code) const {

        VkShaderModuleCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    VkDescriptorSetLayout Core::createComputeDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) const {

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout computeDescriptorSetLayout;
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor set layout!");
        }

        return computeDescriptorSetLayout;
    }

    VkPipeline Core::createComputePipeline(const char *shaderPath) const {

        auto path = SHADER_PATH / fs::path(shaderPath);
        auto computeShaderCode = readFile(path.c_str());
        auto computeShaderModule = createShaderModule(computeShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = computeShaderModule,
                .pName = "main"
        };

        auto computeDescriptorLayout = createComputeDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>{
                VkDescriptorSetLayoutBinding{
                        .binding = 0,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                        .pImmutableSamplers = nullptr
                }
        });

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &computeDescriptorLayout
        };

        VkPipelineLayout computePipelineLayout;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        VkComputePipelineCreateInfo pipelineInfo = {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage = computeShaderStageInfo,
                .layout = computePipelineLayout
        };

        VkPipeline computePipeline;
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(device, computeShaderModule, nullptr);
        return computePipeline;
    }
};