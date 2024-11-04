//
// Created by Yucheng Soku on 2024/10/30.
//

#ifndef VKHYDROCORE_CORE_H
#define VKHYDROCORE_CORE_H

#include <array>
#include <vector>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "Buffer.h"
#include "Pipeline.h"
#include "nlohmann/json.hpp"

using Json = nlohmann::json;

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

    struct ResourceBinding {
        VkDeviceSize    set;
        VkDeviceSize    binding;
        std::string     name;

        static ResourceBinding deserialize(const nlohmann::json& json) {
            return ResourceBinding {
                .set        = json["set"],
                .binding    = json["binding"],
                .name       = json["name"]
            };
        }
    };

    struct ComputePass {
        std::string                  shader;
        std::vector<ResourceBinding> resource;
        std::array<VkDeviceSize, 3>  dispatch;

        ComputePass(std::string& shader, std::vector<ResourceBinding>& resource, std::array<VkDeviceSize, 3>& dispatch)
            : shader(std::move(shader)), resource(std::move(resource)), dispatch(dispatch)
        {}
    };

    class Core {

    public:
        VkInstance                          instance                    = VK_NULL_HANDLE;
        VkPhysicalDevice                    physicalDevice              = VK_NULL_HANDLE;
        VkDevice                            device                      = VK_NULL_HANDLE;
        VkQueue                             computeQueue                = VK_NULL_HANDLE;
        VkCommandPool                       commandPool                 = VK_NULL_HANDLE;
        VkDescriptorPool                    storageDescriptorPool       = VK_NULL_HANDLE;
        VkCommandBuffer                     computeCommandBuffer        = VK_NULL_HANDLE;
        VkSemaphore                         computeFinishedSemaphores   = VK_NULL_HANDLE;
        VkFence                             computeInFlightFences       = VK_NULL_HANDLE;

        std::vector<ComputePass>                                            passList;
        std::unordered_map<std::string, std::unique_ptr<Buffer>>            bufferMap;
        std::unordered_map<std::string, std::unique_ptr<ComputePipeline>>   pipelineMap;

    private:
        VkDebugUtilsMessengerEXT            m_debugMessenger            = VK_NULL_HANDLE;

    public:
        Core();
        ~Core();

        void                                runScript();
        void                                idle() const;
        void                                parseScript(const char* path);
        void                                tick(ComputePipeline* computePipeline);
        void                                copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);

        [[nodiscard]] Buffer                createStagingBuffer(VkDeviceSize size) const;
        ComputePipeline*                    createComputePipeline(const char *shaderPath) const;

        template<typename T>
        Buffer*                             createStorageBuffer(const char* name, const std::vector<T>& data) {
            VkDeviceSize bufferSize = data.size() * sizeof(T);

            auto stagingBuffer = createStagingBuffer(bufferSize);
            stagingBuffer.writeData(data);

            auto storageBuffer = new Buffer(device, name, physicalDevice,
                                 bufferSize,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            copyBuffer(stagingBuffer.buffer, storageBuffer->buffer, bufferSize);

            return storageBuffer;
        }

    private:
        void                                createInstance();
        void                                createCommandPool();
        void                                createSyncObjects();
        void                                pickPhysicalDevice();
        void                                setupDebugMessenger();
        void                                createLogicalDevice();
        void                                createDescriptorPool();
        void                                createComputeCommandBuffer();
        void                                recordComputeCommandBuffer(const ComputePipeline& computePipeline) const;
    };
}
#endif //VKHYDROCORE_CORE_H
