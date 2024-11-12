//
// Created by Yucheng Soku on 2024/10/31.
//

#ifndef VKHYDROCORE_BUFFER_H
#define VKHYDROCORE_BUFFER_H

#include <vector>
#include <utility>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include "config.h"

namespace NextHydro {

    union Flag {
        char        c[4];
        float       f;
        uint32_t    u;
    };

    struct ScopedMemoryMapping {

        const VkDevice&           device;
        void*                     mappedData          = nullptr;
        VkDeviceMemory            memory              = VK_NULL_HANDLE;

        ScopedMemoryMapping(const VkDevice& device, VkDeviceMemory& memory, VkDeviceSize size, size_t offset = 0)
                : device(device), memory(memory)
       {
            if (vkMapMemory(device, memory, offset, size, 0, &mappedData) != VK_SUCCESS) {
                throw std::runtime_error("failed to map memory!");
            }
        };

        ~ScopedMemoryMapping() {
            if (mappedData) vkUnmapMemory(device, memory);
        }
    };

    class Buffer {
    private:
        const VkDevice&     m_device;

    public:
        std::string                 name;
        VkDeviceSize                size           = 0;
        VkBuffer                    buffer         = VK_NULL_HANDLE;
        VkDeviceMemory              memory         = VK_NULL_HANDLE;
        VkBufferUsageFlags          usageFlags     = 0;
        VkDescriptorBufferInfo      descriptorBufferInfo = {};
        VkDescriptorType            descriptorType = static_cast<VkDescriptorType>(0);

        Buffer(const VkDevice& device, std::string name, const VkPhysicalDevice& physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
                : m_device(device), name(std::move(name)), size(size)
        {
            create(physicalDevice, usage, properties);
        }
        ~Buffer() {
            vkDestroyBuffer(m_device, buffer, nullptr);
            vkFreeMemory(m_device, memory, nullptr);
        }

        void writeData(char* pData) {

            auto mappedMemory = ScopedMemoryMapping(m_device, memory, size);
            memcpy(mappedMemory.mappedData, pData, static_cast<size_t>(size));
        } // auto unmap

        template<typename T>
        void readData(std::vector<T>& data) {
            data.resize(size / sizeof(T));
            auto mappedMemory = ScopedMemoryMapping(m_device, memory, size);
            memcpy(data.data(), mappedMemory.mappedData, static_cast<size_t>(size));
        } // auto unmap

        void readFlag(Flag& flag, size_t offset = 0) {
            auto mappedMemory = ScopedMemoryMapping(m_device, memory, 4, offset);
            memcpy(flag.c, mappedMemory.mappedData, 4);
        } // auto unmap

        VkDescriptorBufferInfo& getDescriptorBufferInfo(VkDeviceSize offset = 0, VkDeviceSize range = 0) {

            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range = size;
            if (offset)
                descriptorBufferInfo.offset = offset;
            if (range && range <= size)
                descriptorBufferInfo.range = range;

            return descriptorBufferInfo;
        }

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
            usageFlags = usage;
            if          (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            else if     (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            VkBufferCreateInfo bufferInfo = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    .size = size,
                    .usage = usage,
                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to create buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                    .allocationSize = memRequirements.size,
                    .memoryTypeIndex = Buffer::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties),
            };

            if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate buffer memory");
            }

            vkBindBufferMemory(m_device, buffer, memory, 0);

            descriptorBufferInfo = {
                .buffer = buffer,
                .offset = 0,
                .range = size
            };
        }
    };
}

#endif //VKHYDROCORE_BUFFER_H
