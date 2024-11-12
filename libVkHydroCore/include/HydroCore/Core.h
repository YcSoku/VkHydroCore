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

    struct ComputePass {
        std::string                     shader;
        std::array<uint32_t, 3>         groupCounts;

        ComputePass(std::string& shader, std::array<uint32_t, 3>& groupCounts)
                : shader(std::move(shader)), groupCounts(groupCounts)
        {}
    };

    struct IFlowNode {
        const VkDevice&                             device;
        std::vector<std::shared_ptr<ComputePass>>   passes;

        explicit IFlowNode(const VkDevice& _device, const std::vector<std::shared_ptr<ComputePass>>& passes)
        : device(_device), passes(passes)
        {}

        virtual bool isComplete() = 0;
        virtual char nodeType() = 0;
        virtual void postProcess(const VkCommandBuffer& commandBuffer) = 0;
        virtual ~IFlowNode() = default;
    };

    struct IterableFlowNode : public IFlowNode {
        size_t count;
        size_t currentFrame = 0;

        IterableFlowNode(const VkDevice& _device, const std::vector<std::shared_ptr<ComputePass>>& passes, size_t count)
                : IFlowNode(_device, passes), count(count)
        {}

        bool isComplete() override {
            if (currentFrame ++ <= count)
                return true;
            return false;
        }

        char nodeType() override { return 0b01; }

        void postProcess(const VkCommandBuffer& commandBuffer) override {}
    };

    struct PollableFlowNode : public IFlowNode {
        char                                            type = 0b11;
        std::function<bool()>                           op;
        Flag                                            flag;
        float_t                                         threshold;
        size_t                                          flagIndex;
        std::shared_ptr<Buffer>                         flagBuffer;
        Buffer*                                         stagingBuffer;
        std::function<void(const VkCommandBuffer&)>     postProcessFunc;

        PollableFlowNode(const VkDevice& _device, const std::vector<std::shared_ptr<ComputePass>>& passes, const std::shared_ptr<Buffer>& _flagBuffer, Buffer* _stagingBuffer, const std::string& operation, size_t _flagIndex, float_t _threshold, bool isDiscrete)
            : IFlowNode(_device, passes), flagBuffer(_flagBuffer), stagingBuffer(_stagingBuffer), flagIndex(_flagIndex), threshold(_threshold), flag({.f = 0.0})
        {
            flag.f = 0.0;
            if (operation == "less") {
                op = [this]() { return getData() < threshold; };
            } else if (operation == "lEqual") {
                op = [this]() { return getData() <= threshold; };
            } else if (operation == "greater") {
                op = [this]() { return getData() > threshold; };
            } else if (operation == "gEqual") {
                op = [this]() { return getData() >= threshold; };
            } else {
                op = [this]() { return getData() == threshold; };
            }

            if (isDiscrete) {
                postProcessFunc = [this](const VkCommandBuffer& commandBuffer) { postProcessForDiscreteGPU(commandBuffer); };
            } else {
                postProcessFunc = [](const VkCommandBuffer& commandBuffer) {};
                stagingBuffer = flagBuffer.get();
            }
        }

        [[nodiscard]] float getData() {
            stagingBuffer->readFlag(flag, flagIndex * 4);
            std::cout << "Total time: " << flag.f << std::endl;
            return flag.f;
        }

        bool isComplete() override {
            return op();
        }

        char nodeType() override { return 0b11; }

        void postProcess(const VkCommandBuffer& commandBuffer) override {
            postProcessFunc(commandBuffer);
        }

        void postProcessForDiscreteGPU(const VkCommandBuffer& commandBuffer) const {
            VkBufferCopy copyRegion = {
                    .srcOffset = flagIndex * 4,
                    .dstOffset = 0,
                    .size = 4,
            };
            vkCmdCopyBuffer(commandBuffer, flagBuffer->buffer, stagingBuffer->buffer, 1, &copyRegion);
        }
    };

    class Core {

    private:
        VkDebugUtilsMessengerEXT            m_debugMessenger            = VK_NULL_HANDLE;

    public:
        bool                                isDiscrete                  =   false;
        uint32_t                            currentFenceIndex           =   0;
        uint32_t                            currentCommandBufferIndex   =   0;

        VkDevice                            device                      =   VK_NULL_HANDLE;
        VkInstance                          instance                    =   VK_NULL_HANDLE;
        VkCommandPool                       commandPool                 =   VK_NULL_HANDLE;
        VkQueue                             computeQueue                =   VK_NULL_HANDLE;
        VkDescriptorPool                    descriptorPool              =   VK_NULL_HANDLE;
        VkPhysicalDevice                    physicalDevice              =   VK_NULL_HANDLE;
        VkFence                             computeInFlightFences       =   VK_NULL_HANDLE;
        VkDescriptorPool                    storageDescriptorPool       =   VK_NULL_HANDLE;
        VkSemaphore                         computeFinishedSemaphores   =   VK_NULL_HANDLE;

        std::vector<VkFence>                                                fences;
        std::vector<std::unique_ptr<IFlowNode>>                             flowNode_list;
        std::vector<VkCommandBuffer>                                        commandBuffers;
        std::vector<VkDescriptorSet>                                        descriptorSetPool;
        std::vector<VkCopyDescriptorSet>                                    descriptorCopySets;
        std::vector<VkWriteDescriptorSet>                                   descriptorWriteSets;

        std::unordered_map<std::string, std::shared_ptr<ComputePass>>       name_pass_map;
        std::unordered_map<std::string, std::shared_ptr<Buffer>>            name_buffer_map;
        std::unordered_map<std::string, std::shared_ptr<ComputePipeline>>   name_pipeline_map;
        std::unordered_map<std::string, std::array<uint32_t, 2>>            buffer_descriptorSetPool_Map;

    public:
        Core();
        ~Core();

        // Running Mode <Script-Framework> [ parse -> run ]
        void                                parseScript(const char* path);
        void                                runScript();

        // TODO: Implement running mode with simulation-framework
        // Running Mode <Simulation-Framework> [ initialization -> step -> ... -> step -> output ]
        void                                initialization();
        void                                output();
        void                                step();

        // Basic Operation for Submit [ preheat -> begin -> end -> submit ]
        VkCommandBuffer                     commandBegin();
        void                                commandEnd();
        void                                preheat();
        void                                submit();

        // Basic Operation for Computation
        void                                copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
        static void                         dispatch(const VkCommandBuffer& commandBuffer, const ComputePipeline* pipeline, std::array<uint32_t, 3> groupCounts);
        void                                updateBindings() const;

        // Basic Operation for Synchronization
        void                                idle() const;

        // Create Buffers
        [[nodiscard]] Buffer                createTempStagingBuffer(VkDeviceSize size) const;
        void                                createUniformBuffer(const std::string& name, Buffer*& uniformBuffer, Block& blockMemory);
        void                                createStorageBuffer(const std::string& name, Buffer*& storageBuffer, Block& blockMemory);
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
    };
}
#endif //VKHYDROCORE_CORE_H
