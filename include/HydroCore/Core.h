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
#include "Block.h"
#include "Buffer.h"
#include "Pipeline.h"
#include "CommandNode.h"
#include "nlohmann/json.hpp"

using Json = nlohmann::json;
namespace NextHydro {

    class Core {

    private:
        VkDebugUtilsMessengerEXT            m_debugMessenger                = VK_NULL_HANDLE;

    public:
        bool                                isDiscrete                      =   false;
        uint32_t                            currentFenceIndex               =   0;
        uint32_t                            currentCommandBufferIndex       =   0;
        uint32_t                            maxComputeWorkGroupInvocations  =   0;

        VkDevice                            device                          =   VK_NULL_HANDLE;
        VkInstance                          instance                        =   VK_NULL_HANDLE;
        VkCommandPool                       commandPool                     =   VK_NULL_HANDLE;
        VkQueue                             computeQueue                    =   VK_NULL_HANDLE;
        VkDescriptorPool                    descriptorPool                  =   VK_NULL_HANDLE;
        VkPhysicalDevice                    physicalDevice                  =   VK_NULL_HANDLE;

        std::vector<std::unique_ptr<ICommandNode>>                          flowNode_list;
        std::vector<VkFence>                                                fences;
        std::vector<VkCommandBuffer>                                        commandBuffers;
        std::vector<VkDescriptorSet>                                        descriptorSetPool;
        std::vector<VkCopyDescriptorSet>                                    descriptorCopySets;
        std::vector<VkWriteDescriptorSet>                                   descriptorWriteSets;

        std::unordered_map<std::string, std::shared_ptr<ComputePass>>       name_pass_map;
        std::unordered_map<std::string, std::shared_ptr<Buffer>>            name_buffer_map;
        std::unordered_map<std::string, std::shared_ptr<ComputePipeline>>   name_pipeline_map;
        std::unordered_map<std::string, std::array<uint32_t, 2>>            buffer_descriptorSetPool_map;

    public:
        Core();
        ~Core();

        // Running Mode <Script-Framework> [ parse -> run ]
        void                                parseScript(const std::string& path);
        void                                runScript();

        // TODO: Implement running mode with simulation-framework
        // Running Mode <Simulation-Framework> [ initialization -> step -> ... -> step -> output ]
        void                                initialization(const std::string& path);
        void                                output();
        bool                                step();

        // Command Node execution
        void                                executeNode(ICommandNode* node);

        // Basic Operation for Computation
        void                                copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) const;
        static void                         dispatch(const VkCommandBuffer& commandBuffer, const ComputePipeline* pipeline, std::array<uint32_t, 3> groupCounts);
        void                                updateBindings() const;

        // Basic Operation for Synchronization
        void                                idle() const;

        // Create Buffers
        [[nodiscard]] Buffer                createTempStagingBuffer(VkDeviceSize size) const;
        void                                createUniformBuffer(const std::string& name, Buffer*& uniformBuffer, Block& blockMemory) const;
        void                                createStorageBuffer(const std::string& name, Buffer*& storageBuffer, Block& blockMemory) const;
        void                                createStagingBuffer(const std::string& name, Buffer*& uniformBuffer, VkDeviceSize size) const;

    private:

        // Functions for Core Creation
        void                                createFence();
        void                                createInstance();
        void                                createCommandPool();
        void                                createSyncObjects();
        void                                pickPhysicalDevice();
        void                                setupDebugMessenger();
        void                                createLogicalDevice();
        void                                createCommandBuffer();

        // Basic Operation for Node execution [ preheat -> begin -> end -> submit ]
        VkCommandBuffer                     commandBegin();
        void                                commandEnd();
        void                                preheat();
        void                                submit();
    };
}
#endif //VKHYDROCORE_CORE_H
