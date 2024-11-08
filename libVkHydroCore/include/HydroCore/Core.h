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
        std::string                     shader;
        std::vector<ResourceBinding>    resource;
        std::array<uint32_t, 3>         groupCounts;

        ComputePass(std::string& shader, std::vector<ResourceBinding>& resource, std::array<uint32_t, 3>& groupCounts)
                : shader(std::move(shader)), resource(std::move(resource)), groupCounts(groupCounts)
        {}
    };

    struct IFlowNode {
        std::vector<std::shared_ptr<ComputePass>> passes;

        explicit IFlowNode(const std::vector<std::shared_ptr<ComputePass>>& passes)
        : passes(passes)
        {}

        virtual bool isComplete() = 0;
        virtual char nodeType() = 0;
        virtual ~IFlowNode() = default;
    };

    struct IterableFlowNode : public IFlowNode {
        size_t count;
        size_t currentFrame = 0;

        IterableFlowNode(const std::vector<std::shared_ptr<ComputePass>>& passes, size_t count)
                : IFlowNode(passes), count(count)
        {}

        bool isComplete() override {
            if (currentFrame ++ <= count)
                return true;
            return false;
        }

        char nodeType() override { return 0b01; }
    };

    struct FlagFlowNode : public IFlowNode {
        char type = 0b11;
        float_t flag;
        size_t flagIndex;
        std::function<bool()> op;
        std::shared_ptr<Buffer> flagBuffer;

        FlagFlowNode(const std::vector<std::shared_ptr<ComputePass>>& passes, const std::shared_ptr<Buffer>& _flagBuffer, const std::string& operation, size_t _flagIndex, float_t _flag)
            : IFlowNode(passes), flagBuffer(_flagBuffer), flagIndex(_flagIndex), flag(_flag)
        {

            if (operation == "less") {
                op = [this]() { return getData() < flag; };
            } else if (operation == "lEqual") {
                op = [this]() { return getData() <= flag; };
            } else if (operation == "greater") {
                op = [this]() { return getData() > flag; };
            } else if (operation == "gEqual") {
                op = [this]() { return getData() >= flag; };
            } else {
                op = [this]() { return getData() == flag; };
            }
        }

        [[nodiscard]] float getData() const {

            std::vector<float> step;
            flagBuffer->readData(step);
            float currentStep = step[flagIndex];
            std::cout << step[0] << std::endl;
            return currentStep;
        }

        bool isComplete() override {
            return op();
        }

        char nodeType() override { return 0b11; }
    };

    class Core {

    public:
        VkInstance                          instance                    =   VK_NULL_HANDLE;
        VkPhysicalDevice                    physicalDevice              =   VK_NULL_HANDLE;
        VkDevice                            device                      =   VK_NULL_HANDLE;
        VkQueue                             computeQueue                =   VK_NULL_HANDLE;
        VkCommandPool                       commandPool                 =   VK_NULL_HANDLE;
        VkDescriptorPool                    storageDescriptorPool       =   VK_NULL_HANDLE;
        VkSemaphore                         computeFinishedSemaphores   =   VK_NULL_HANDLE;
        VkFence                             computeInFlightFences       =   VK_NULL_HANDLE;
        uint32_t                            currentCommandBufferIndex   =   0;
        uint32_t                            currentFenceIndex           =   0;
        std::vector<VkCommandBuffer>        commandBuffers;
        std::vector<VkFence>                fences;

        std::vector<ComputePass>                                            passList;
        std::unordered_map<std::string, std::shared_ptr<ComputePass>>       passMap;
        std::unordered_map<std::string, std::shared_ptr<Buffer>>            bufferMap;
        std::unordered_map<std::string, std::shared_ptr<ComputePipeline>>   pipelineMap;
        std::vector<std::unique_ptr<IFlowNode>>                             flowNodeList;

    private:
        VkDebugUtilsMessengerEXT            m_debugMessenger            = VK_NULL_HANDLE;

    public:
        Core();
        ~Core();

        void                                commandBegin();
        void                                dispatch(const ComputePipeline* pipeline, std::array<uint32_t, 3> groupCounts);
        void                                commandEnd();
        void                                runScript();
        void                                preheat();
        void                                submit();
        void                                idle() const;
        void                                parseScript(const char* path);
//        void                                tick(ComputePipeline *computePipeline, VkDeviceSize x, VkDeviceSize y, VkDeviceSize z);
        void                                copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);

        [[nodiscard]] Buffer                createStagingBuffer(VkDeviceSize size) const;
        ComputePipeline*                    createComputePipeline(const char *shaderPath) const;

        void                                createUniformBuffer(const std::string& name, Buffer*& uniformBuffer, Block& blockMemory);
        void                                createStorageBuffer(const std::string& name, Buffer*& storageBuffer, Block& blockMemory);

    private:
        void                                createFence();
        void                                createInstance();
        void                                createCommandPool();
        void                                createSyncObjects();
        void                                pickPhysicalDevice();
        void                                setupDebugMessenger();
        void                                createLogicalDevice();
        void                                createDescriptorPool();
        size_t                              createCommandBuffer();
//        void                                recordComputeCommandBuffer(const ComputePipeline& computePipeline) const;
    };
}
#endif //VKHYDROCORE_CORE_H
