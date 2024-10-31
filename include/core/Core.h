//
// Created by Yucheng Soku on 2024/10/30.
//

#ifndef VKHYDROCORE_CORE_H
#define VKHYDROCORE_CORE_H

#include <vector>
#include <optional>
#include <vulkan/vulkan.h>

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

    struct ScopedMemoryMapping {

        const VkDevice&           device;
        void*                     mappedData          = nullptr;
        VkDeviceMemory            memory              = VK_NULL_HANDLE;

        ScopedMemoryMapping(const VkDevice& device, VkDeviceMemory& memory, VkDeviceSize size)
                : device(device), memory(memory)
        {
            if (vkMapMemory(device, memory, 0, size, 0, &mappedData) != VK_SUCCESS) {
                throw std::runtime_error("failed to map memory!");
            }
        };

        ~ScopedMemoryMapping() {
            if (mappedData) vkUnmapMemory(device, memory);
        }
    };

    class Buffer {
    private:
        const VkDevice&     device;

    public:
        VkDeviceSize        size           = 0;
        VkBuffer            buffer         = VK_NULL_HANDLE;
        VkDeviceMemory      memory         = VK_NULL_HANDLE;

        Buffer(const VkDevice& device, const VkPhysicalDevice& physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
                : device(device), size(size)
        {
            create(physicalDevice, usage, properties);
        }
        ~Buffer() {
            vkDestroyBuffer(device, buffer, nullptr);
            vkFreeMemory(device, memory, nullptr);
        }

        template<typename T>
        void writeData(const std::vector<T>& data) {

            auto mappedMemory = ScopedMemoryMapping(device, memory, size);
            memcpy(mappedMemory.mappedData, data.data(), static_cast<size_t>(size));
        } // auto unmap

    private:

        static uint32_t findMemoryType(const VkPhysicalDevice& physicalDevice,uint32_t typeFilter, VkMemoryPropertyFlags properties) {

            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

            uint32_t index;
            for (index = 0; index < memProperties.memoryTypeCount; ++index) {
                if ((typeFilter & (1 << index)) && (memProperties.memoryTypes[index].propertyFlags & properties) == properties) {
                    return index;
                }
            }
            return index;
        }

        void create(
                const VkPhysicalDevice&         physicalDevice,
                VkBufferUsageFlags              usage,
                VkMemoryPropertyFlags           properties
        ) {
            VkBufferCreateInfo bufferInfo = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    .size = size,
                    .usage = usage,
                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to create buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                    .allocationSize = memRequirements.size,
                    .memoryTypeIndex = Buffer::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties),
            };

            if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate buffer memory");
            }

            vkBindBufferMemory(device, buffer, memory, 0);
        }
    };

    class Core {

    public:
        VkInstance                      instance            = VK_NULL_HANDLE;
        VkPhysicalDevice                physicalDevice      = VK_NULL_HANDLE;
        VkDevice                        device              = VK_NULL_HANDLE;
        VkQueue                         computeQueue        = VK_NULL_HANDLE;
        VkCommandPool                   commandPool         = VK_NULL_HANDLE;

    private:
        VkDebugUtilsMessengerEXT        m_debugMessenger    = VK_NULL_HANDLE;

    public:
        Core();
        ~Core();

        [[nodiscard]] Buffer            createStagingBuffer(VkDeviceSize size) const;
        [[nodiscard]] VkPipeline        createComputePipeline(const char *shaderPath) const;

        void                            copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);

        template<typename T>
        Buffer                          createStorageBuffer(const std::vector<T>& data) {
            VkDeviceSize bufferSize = data.size() * sizeof(T);

            auto stagingBuffer = createStagingBuffer(bufferSize);
            stagingBuffer.writeData(data);

            Buffer storageBuffer(device, physicalDevice,
                                 bufferSize,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
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

        [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;
        [[nodiscard]] VkDescriptorSetLayout createComputeDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) const;
    };
}
#endif //VKHYDROCORE_CORE_H
