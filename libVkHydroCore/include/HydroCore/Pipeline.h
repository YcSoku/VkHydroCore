//
// Created by Yucheng Soku on 2024/11/1.
//

#ifndef VKHYDROCORE_PIPELINE_H
#define VKHYDROCORE_PIPELINE_H

#include <tuple>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <unordered_map>
#include "config.h"
#include "vulkan/vulkan.h"
#include "spirv_reflect.h"
#include "nlohmann/json.hpp"
#include "shaderc/shaderc.hpp"

struct DescriptorKey {
    uint32_t set;
    uint32_t binding;

    bool operator==(const DescriptorKey& other) const {
        return std::tie(set, binding) == std::tie(other.set, other.binding);
    }
};

namespace std {
    template <>
    struct hash<DescriptorKey> {
        std::size_t operator()(const DescriptorKey& key) const {

            std::size_t hashSet = std::hash<uint32_t>()(key.set);
            std::size_t hashBinding = std::hash<uint32_t>()(key.binding);
            return hashSet ^ (hashBinding << 1);
        }
    };
}

using Json = nlohmann::json;
namespace fs = std::filesystem;
namespace NextHydro {

    static std::vector<uint32_t> compileGLSLtoSPIRV(const std::string& glslCode, shaderc_shader_kind shaderType) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetOptimizationLevel(shaderc_optimization_level_size);

        shaderc::CompilationResult result = compiler.CompileGlslToSpv(
                glslCode.c_str(),
                glslCode.size(),
                shaderType,
                "shader.glsl",
                options
        );

        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cerr << "Shader compilation failed: " << result.GetErrorMessage() << std::endl;
            return {};
        }

        return {result.cbegin(), result.cend()};
    }

    class ReflectShaderModule {
    public:
        SpvReflectShaderModule          prototypeModule{};

    public:
        explicit ReflectShaderModule(const std::vector<uint32_t>& spirvCode) {

            auto result = spvReflectCreateShaderModule(spirvCode.size() * sizeof(uint32_t), spirvCode.data(), &prototypeModule);
            assert(result == SPV_REFLECT_RESULT_SUCCESS);
        }

        ~ReflectShaderModule() {

            spvReflectDestroyShaderModule(&prototypeModule);
        }
    };

    class ShaderModule {
    private:
        const VkDevice&         m_device;
    public:
        ReflectShaderModule     reflector;
        VkShaderModule          module      = VK_NULL_HANDLE;

        ShaderModule(const VkDevice& device, const std::vector<uint32_t>& spirvCode)
                : m_device(device), reflector(ReflectShaderModule(spirvCode))
        {
            createShaderModule(spirvCode);
        }

        ~ShaderModule() {
            if (module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(m_device, module, nullptr);
            }
        }

        VkPipelineShaderStageCreateInfo getShaderStageCreateInfo() {
            return {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = static_cast<VkShaderStageFlagBits>(reflector.prototypeModule.shader_stage),
                    .module = module,
                    .pName = reflector.prototypeModule.entry_point_name
            };
        }

        void generateDescriptorSetLayout(std::vector<VkDescriptorSetLayout>& descriptorSetLayout) {

            descriptorSetLayout.resize(reflector.prototypeModule.descriptor_set_count);
            for (size_t i = 0; i < reflector.prototypeModule.descriptor_set_count; ++i) {
                auto binding_count = reflector.prototypeModule.descriptor_sets[i].binding_count;
                auto bindingInfo = std::vector<VkDescriptorSetLayoutBinding>(binding_count);

                for (size_t j = 0; j < binding_count; ++j) {
                    auto binding = reflector.prototypeModule.descriptor_sets[i].bindings[j];
                    bindingInfo[j] = VkDescriptorSetLayoutBinding{
                            .binding = binding->binding,
                            .descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type),
                            .descriptorCount = binding->count,
                            .stageFlags = reflector.prototypeModule.shader_stage,
                            .pImmutableSamplers = nullptr
                    };
                }
                VkDescriptorSetLayoutCreateInfo layoutInfo{};
                layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layoutInfo.bindingCount = bindingInfo.size();
                layoutInfo.pBindings = bindingInfo.data();

                if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &descriptorSetLayout[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create compute descriptor set layout!");
                }
            }

        }

    private:
        void createShaderModule(const std::vector<uint32_t >& code) {

            VkShaderModuleCreateInfo createInfo = {
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .codeSize = code.size() * sizeof(uint32_t),
                    .pCode = reinterpret_cast<const uint32_t*>(code.data())
            };

            if (vkCreateShaderModule(m_device, &createInfo, nullptr, &module) != VK_SUCCESS) {
                throw std::runtime_error("failed to create shader module!");
            }
        }
    };

    class IPipeline {
    protected:
        const VkDevice&                             m_device;

    public:
        std::string                                 name;
        VkPipeline                                  pipeline                =           VK_NULL_HANDLE;
        VkPipelineLayout                            pipelineLayout          =           VK_NULL_HANDLE;
        VkDescriptorPool                            descriptorPool          =           VK_NULL_HANDLE;
        std::unordered_map<DescriptorKey, size_t>   descriptorMap;
        std::vector<VkDescriptorSet>                descriptorSets;
        std::vector<VkWriteDescriptorSet>           descriptorSetWrite;
        std::vector<VkDescriptorSetLayout>          descriptorSetLayout;

        ~IPipeline() {

            if (descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device, descriptorPool, nullptr);
            }

            for (const auto& layout : descriptorSetLayout) {
                if (layout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
                }
            }

            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
            }

            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device, pipeline, nullptr);
            }
        }

        size_t findDescriptorSetWriteIndex(uint32_t dstSet, uint32_t dstBinding) {
            DescriptorKey key = { dstSet, dstBinding };
            auto it = descriptorMap.find(key);
            if (it != descriptorMap.end()) {
                return it->second;
            }
            throw std::runtime_error("descriptor set write not found!");
        }

    protected:
        explicit IPipeline(const VkDevice& device, const char* name)
                : m_device(device), name(name)
        {}
    };

    class ComputePipeline : public IPipeline {
    public:
        ComputePipeline(const VkDevice& device, const char* name, const char *glslCode)
                : IPipeline(device, name)
        {
            create(glslCode);
        }

        void bindBuffer(size_t set, size_t binding, Buffer* buffer, VkDeviceSize offset = 0, VkDeviceSize range = 0){

            size_t index = findDescriptorSetWriteIndex(set, binding);
            descriptorSetWrite[index].pBufferInfo = &buffer->getDescriptorBufferInfo(offset, range);
        }

        void tick() {

            vkUpdateDescriptorSets(m_device, descriptorSetWrite.size(), descriptorSetWrite.data(), 0, nullptr);
        }

    private:
        void create(const char *glslCode) {

            // Compile to Spir-V code and build shader module
            auto spirvCode = compileGLSLtoSPIRV(glslCode, shaderc_compute_shader);
            ShaderModule computeShaderModule(m_device, spirvCode);

            // Generate descriptor set layout for pipeline from shader module
            computeShaderModule.generateDescriptorSetLayout(descriptorSetLayout);

            // Create pipeline layout
            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = static_cast<uint32_t>(descriptorSetLayout.size()),
                    .pSetLayouts = descriptorSetLayout.data()
            };
            if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create compute pipeline layout!");
            }

            // Create pipeline
            VkComputePipelineCreateInfo pipelineInfo = {
                    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                    .stage = computeShaderModule.getShaderStageCreateInfo(),
                    .layout = pipelineLayout
            };
            if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
                throw std::runtime_error("failed to create compute pipeline!");
            }

            // Generate descriptor pool
            // Count all bindings
            uint32_t poolSizeCount = 0;
            uint32_t storageBufferNum = 0;
            auto bindingCount = computeShaderModule.reflector.prototypeModule.descriptor_binding_count;
            const auto& bindings = computeShaderModule.reflector.prototypeModule.descriptor_bindings;

            for(size_t i = 0; i < bindingCount; ++i) {
                switch (bindings[i].descriptor_type) {
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        if (storageBufferNum == 0) poolSizeCount ++;
                        storageBufferNum ++;
                        break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                        break;
                }
            }

            size_t index = 0;
            std::vector<VkDescriptorPoolSize> poolSizes(poolSizeCount);
            if (storageBufferNum) poolSizes[index++] = {
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .descriptorCount = storageBufferNum
                };

            VkDescriptorPoolCreateInfo poolInfo = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .maxSets = computeShaderModule.reflector.prototypeModule.descriptor_set_count,
                    .poolSizeCount = poolSizeCount,
                    .pPoolSizes = poolSizes.data(),
            };

            if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor pool!");
            }

            // Allocate descriptor set in GPU
            VkDescriptorSetAllocateInfo allocInfo = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = descriptorPool,
                    .descriptorSetCount = static_cast<uint32_t>(descriptorSetLayout.size()),
                    .pSetLayouts = descriptorSetLayout.data()
            };

            descriptorSets.resize(computeShaderModule.reflector.prototypeModule.descriptor_set_count);
            if (vkAllocateDescriptorSets(m_device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }

            descriptorSetWrite.resize(bindingCount);
            for(size_t i = 0; i < bindingCount; ++i) {
                const auto& binding = bindings[i];
                descriptorSetWrite[i] = VkWriteDescriptorSet {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = descriptorSets[binding.set],
                        .dstBinding = binding.binding,
                        .dstArrayElement = binding.input_attachment_index,
                        .descriptorCount = binding.count,
                        .descriptorType = static_cast<VkDescriptorType>(binding.descriptor_type)
                };

                DescriptorKey key = {
                        .set = binding.set,
                        .binding = binding.binding
                };
                descriptorMap[key] = i;
            }
        }
    };
}

#endif //VKHYDROCORE_PIPELINE_H
