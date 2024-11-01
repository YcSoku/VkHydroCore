//
// Created by Yucheng Soku on 2024/10/30.
//

#ifndef VKHYDROCORE_CORE_H
#define VKHYDROCORE_CORE_H

#include <vector>
#include <optional>
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "Pipeline.h"

namespace NextHydro {

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

    class Core {

    public:
        VkInstance                      instance                    = VK_NULL_HANDLE;
        VkPhysicalDevice                physicalDevice              = VK_NULL_HANDLE;
        VkDevice                        device                      = VK_NULL_HANDLE;
        VkQueue                         computeQueue                = VK_NULL_HANDLE;
        VkCommandPool                   commandPool                 = VK_NULL_HANDLE;
        VkDescriptorPool                storageDescriptorPool       = VK_NULL_HANDLE;
        VkCommandBuffer                 computeCommandBuffer        = VK_NULL_HANDLE;
        VkSemaphore                     computeFinishedSemaphores   = VK_NULL_HANDLE;
        VkFence                         computeInFlightFences       = VK_NULL_HANDLE;

    private:
        VkDebugUtilsMessengerEXT        m_debugMessenger            = VK_NULL_HANDLE;

    public:
        Core();
        ~Core();

        [[nodiscard]] Buffer            createStagingBuffer(VkDeviceSize size) const;
        [[nodiscard]] ComputePipeline   createComputePipeline(const char *shaderPath) const;
        [[nodiscard]] VkDescriptorSet   createDescriptorSets(const ComputePipeline& pipeline, const Buffer& buffer);
        void                            copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
        void                            tick(const ComputePipeline& computePipeline, const VkDescriptorSet& descriptorSet);

        template<typename T>
        Buffer                          createStorageBuffer(const std::vector<T>& data) {
            VkDeviceSize bufferSize = data.size() * sizeof(T);

            auto stagingBuffer = createStagingBuffer(bufferSize);
            stagingBuffer.writeData(data);

            Buffer storageBuffer(device, physicalDevice,
                                 bufferSize,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            copyBuffer(stagingBuffer.buffer, storageBuffer.buffer, bufferSize);

            return storageBuffer;
        }

    private:
        void                            createInstance();
        void                            createCommandPool();
        void                            pickPhysicalDevice();
        void                            setupDebugMessenger();
        void                            createLogicalDevice();
        void                            createDescriptorPool();
        void                            createComputeCommandBuffer();
        void                            createSyncObjects();
        void                            recordComputeCommandBuffer(const ComputePipeline& computePipeline, const VkDescriptorSet& descriptorSet) const;
    };
}
#endif //VKHYDROCORE_CORE_H
