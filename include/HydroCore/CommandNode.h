//
// Created by Yucheng Soku on 2024/11/14.
//

#ifndef HYDROCOREPLAYER_COMMANDNODE_H
#define HYDROCOREPLAYER_COMMANDNODE_H

#include <utility>
#include <vector>
#include <string>
#include <cassert>
#include <iostream>
#include <functional>
#include <vulkan/vulkan.h>
#include "Types.h"
#include "Buffer.h"
#include "nlohmann/json.hpp"

using Json = nlohmann::json;
namespace NextHydro {

    struct ComputePass {
        std::string                     shader;
        std::array<uint32_t, 3>         groupCounts;

        ComputePass(std::string& shader, std::array<uint32_t, 3>& groupCounts)
                : shader(std::move(shader)), groupCounts(groupCounts)
        {}
    };
    struct ICommandNode {
        std::string                                 name;
        const VkDevice&                             device;
        std::vector<std::shared_ptr<ComputePass>>   passes;

        explicit ICommandNode(std::string _name, const VkDevice& _device, const std::vector<std::shared_ptr<ComputePass>>& passes)
                : name(std::move(_name)), device(_device), passes(passes)
        {}

        virtual bool isComplete() = 0;
        virtual char nodeType() = 0;
        virtual void tick() = 0;
        virtual void postProcess(const VkCommandBuffer& commandBuffer) = 0;
        virtual ~ICommandNode() = default;
    };

    struct IterableCommandNode : public ICommandNode {
        size_t count;
        size_t currentFrame = 0;

        IterableCommandNode(std::string _name, const VkDevice& _device, const std::vector<std::shared_ptr<ComputePass>>& passes, size_t count)
                : ICommandNode(std::move(_name), _device, passes), count(count)
        {}

        void tick() override {}

        bool isComplete() override {
            if (currentFrame ++ <= count)
                return false;
            return true;
        }

        char nodeType() override { return 0b01; }

        void postProcess(const VkCommandBuffer& commandBuffer) override {}
    };

    struct PollableCommandNode : public ICommandNode {
        char                                            type = 0b11;
        std::function<bool()>                           op;
        Flag                                            flag;
        float_t                                         threshold;
        size_t                                          flagIndex;
        size_t                                          stagingIndex;
        std::shared_ptr<Buffer>                         flagBuffer;
        Buffer*                                         stagingBuffer;
        std::function<void(const VkCommandBuffer&)>     postProcessFunc;

        PollableCommandNode(std::string _name, const VkDevice& _device, const std::vector<std::shared_ptr<ComputePass>>& passes, const std::shared_ptr<Buffer>& _flagBuffer, Buffer* _stagingBuffer, const std::string& operation, size_t _flagIndex, float_t _threshold, bool isDiscrete)
                : ICommandNode(std::move(_name), _device, passes), flagBuffer(_flagBuffer), stagingBuffer(_stagingBuffer), flagIndex(_flagIndex), threshold(_threshold), flag()
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
                stagingIndex = 0;
            } else {
                postProcessFunc = [](const VkCommandBuffer& commandBuffer) {};
                stagingBuffer = flagBuffer.get();
                stagingIndex = flagIndex;
            }
        }

        [[nodiscard]] float getData() {
            stagingBuffer->readFlag(flag, stagingIndex * 4);
            std::cout << "Total time: " << flag.f << std::endl;
            return flag.f;
        }

        void tick() override {}

        bool isComplete() override {
            return !op();
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
}

#endif //HYDROCOREPLAYER_COMMANDNODE_H
