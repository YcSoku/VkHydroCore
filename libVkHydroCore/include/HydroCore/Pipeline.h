//
// Created by Yucheng Soku on 2024/11/1.
//

#ifndef VKHYDROCORE_PIPELINE_H
#define VKHYDROCORE_PIPELINE_H

#include <tuple>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <unordered_map>
#include "config.h"
#include "Buffer.h"
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

    static std::vector<uint32_t> compileGLSLtoSPIRV(const std::string& glslCode, shaderc_shader_kind shaderType, bool debugInfo = false) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetOptimizationLevel(shaderc_optimization_level_size);
        if (debugInfo) options.SetGenerateDebugInfo();
        options.SetTargetSpirv(shaderc_spirv_version_1_5);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

        shaderc::CompilationResult result = compiler.CompileGlslToSpv(
                glslCode.c_str(),
                glslCode.size(),
                shaderType,
                "shader.comp",
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
        ReflectShaderModule*    reflector;
        VkShaderModule          module      = VK_NULL_HANDLE;

        ShaderModule(const VkDevice& device, const std::string& glslCode)
                : m_device(device)
        {
            auto spirvCode = compileGLSLtoSPIRV(glslCode, shaderc_compute_shader);
            auto spirvCodeDebug = compileGLSLtoSPIRV(glslCode, shaderc_compute_shader, true);
            reflector = new ReflectShaderModule(spirvCodeDebug);
            createShaderModule(spirvCode);
        }

        ~ShaderModule() {

            delete reflector;
            if (module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(m_device, module, nullptr);
            }
        }

        VkPipelineShaderStageCreateInfo getShaderStageCreateInfo() {
            return {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = static_cast<VkShaderStageFlagBits>(reflector->prototypeModule.shader_stage),
                    .module = module,
                    .pName = reflector->prototypeModule.entry_point_name
            };
        }

        void generateDescriptorSetLayout(std::vector<VkDescriptorSetLayout>& descriptorSetLayout) {

            descriptorSetLayout.resize(reflector->prototypeModule.descriptor_set_count);
            for (size_t i = 0; i < reflector->prototypeModule.descriptor_set_count; ++i) {
                auto binding_count = reflector->prototypeModule.descriptor_sets[i].binding_count;
                auto bindingInfo = std::vector<VkDescriptorSetLayoutBinding>(binding_count);

                for (size_t j = 0; j < binding_count; ++j) {
                    auto binding = reflector->prototypeModule.descriptor_sets[i].bindings[j];
                    bindingInfo[j] = VkDescriptorSetLayoutBinding{
                            .binding = binding->binding,
                            .descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type),
                            .descriptorCount = binding->count,
                            .stageFlags = reflector->prototypeModule.shader_stage,
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

        [[nodiscard]] BufferMemory* fillUniformBlock(const std::string& blockName, const Json& json) const {

            SpvReflectBlockVariable* block;
            const auto bindings = reflector->prototypeModule.descriptor_bindings;
            const size_t bindingCount = reflector->prototypeModule.descriptor_binding_count;
            for (size_t i = 0; i < bindingCount; ++i) {
                block = &bindings[i].block;
                if(spvReflectBlockVariableTypeName(block) == blockName) {
                    break;
                }
            }

            auto buffer = new BufferMemory(block->size);
            const auto& blocksInfo = json["block"];

            for(size_t i = 0; i < block->member_count; ++i) {
                const auto& blockInfo = blocksInfo[i];
                const auto& member = block->members[i];

                void* begin = (char*)buffer->bufferMemory + member.offset;
                switch (type_map[blockInfo["type"].get<std::string>()]) {
                    case 0: { // f32
                        auto value = blockInfo["data"].get<float_t>();
                        std::memcpy(begin, &value, sizeof(float_t));
                        break;
                    }
                    case 1: { // vec2
                        auto value = blockInfo["data"].get<std::array<float_t, 2>>();
                        std::memcpy(begin, &value, sizeof(float_t) * 2);
                        break;
                    }
                    case 2: { // vec3
                        auto value = blockInfo["data"].get<std::array<float_t, 3>>();
                        std::memcpy(begin, &value, sizeof(float_t) * 3);
                        break;
                    }
                    case 3: { // vec4
                        auto value = blockInfo["data"].get<std::array<float_t, 4>>();
                        std::memcpy(begin, &value, sizeof(float_t) * 4);
                        break;
                    }
                    default:
                        break;
                }
            }
            return buffer;
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
        size_t                                      commandBufferIndex      =           0;
        VkPipeline                                  pipeline                =           VK_NULL_HANDLE;
        VkPipelineLayout                            pipelineLayout          =           VK_NULL_HANDLE;
        VkDescriptorPool                            descriptorPool          =           VK_NULL_HANDLE;
        std::unordered_map<DescriptorKey, size_t>   descriptorMap;
        std::vector<VkDescriptorSet>                descriptorSets;
        std::vector<VkWriteDescriptorSet>           descriptorSetWrite;
        std::vector<VkDescriptorSetLayout>          descriptorSetLayout;
        std::vector<std::string>                    bindingResourceNames;
        std::vector<std::array<uint32_t, 2>>        bindingResourceInfo;

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
        ShaderModule* computeShaderModule = nullptr;

    public:
        ComputePipeline(const VkDevice& device, const char* name, const char *glslCode)
                : IPipeline(device, name)
        {
            create(glslCode);
        }

        ~ComputePipeline() {
            delete computeShaderModule;
        }

    private:
        void create(const char *glslCode) {

            // Build shader module
            computeShaderModule = new ShaderModule(m_device, glslCode);

            // Generate descriptor set layout for pipeline from shader module
            computeShaderModule->generateDescriptorSetLayout(descriptorSetLayout);

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
                    .stage = computeShaderModule->getShaderStageCreateInfo(),
                    .layout = pipelineLayout
            };
            if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
                throw std::runtime_error("failed to create compute pipeline!");
            }

            // Resize descriptor set container
            const auto& reflector = computeShaderModule->reflector->prototypeModule;
            descriptorSets.resize(computeShaderModule->reflector->prototypeModule.descriptor_set_count);

            // Reflect binding info (name, set index and binding index used by the shader)
            bindingResourceInfo.resize(reflector.descriptor_binding_count);
            bindingResourceNames.resize(reflector.descriptor_binding_count);
            for (size_t i = 0; i < reflector.descriptor_binding_count; ++i) {

                if (reflector.descriptor_bindings[i].name != std::string("")) {
                    bindingResourceNames[i] = reflector.descriptor_bindings[i].name;
                } else if (reflector.descriptor_bindings[i].type_description->members[0].struct_member_name != std::string("")) {
                    bindingResourceNames[i] = reflector.descriptor_bindings[i].type_description->members[0].struct_member_name;
                } else {
                    throw std::runtime_error("no suitable buffer name reflected from shader code.");
                }

                uint32_t binding = reflector.descriptor_bindings[i].binding;
                uint32_t set = reflector.descriptor_bindings[i].set;
                bindingResourceInfo[i] = { set, binding };
            }
            const int m = 0;
        }
    };
}

#endif //VKHYDROCORE_PIPELINE_H
