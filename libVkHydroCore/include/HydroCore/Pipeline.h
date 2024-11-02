//
// Created by Yucheng Soku on 2024/11/1.
//

#ifndef VKHYDROCORE_PIPELINE_H
#define VKHYDROCORE_PIPELINE_H

#include <vector>
#include <fstream>
#include <filesystem>
#include <vulkan/vulkan.h>
#include "config.h"
#include "nlohmann/json.hpp"
#include "shaderc/shaderc.hpp"

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

    static std::vector<char> readFile(const char* filename) {

        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + std::string(filename));
        }

        std::streamsize fileSize = file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    static Json readJsonFile(const fs::path& jsonPath) {

        std::ifstream iStream(jsonPath);

        if (!iStream) {
            throw std::runtime_error("failed to open JSON file: " + jsonPath.string());
        }

        Json json;
        try {
            iStream >> json;
        } catch (const std::exception& e) {
            throw std::runtime_error("failed to parse JSON file: " + jsonPath.string() + ", error: " + e.what());
        }

        return json;
    }

    class ShaderModule {
    private:
        const VkDevice&     m_device;
    public:
        VkShaderModule      module      = VK_NULL_HANDLE;

//        ShaderModule(const VkDevice& device, const char* shaderPath)
//                : m_device(device)
//        {
//
//            auto path = SHADER_PATH / fs::path(shaderPath);
//            auto shaderCode = readFile(path.c_str());
//            createShaderModule(shaderCode);
//        }

        ShaderModule(const VkDevice& device, const std::vector<uint32_t>& spirvCode)
                : m_device(device)
        {

            createShaderModule(spirvCode);
        }

        ~ShaderModule() {
            if (module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(m_device, module, nullptr);
            }
        }

        ShaderModule(ShaderModule&& other) noexcept
                : m_device(other.m_device), module(other.module)
        {
            other.module = VK_NULL_HANDLE;
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
        const VkDevice&     m_device;

    public:
        VkPipeline                              pipeline            =           VK_NULL_HANDLE;
        VkPipelineLayout                        pipelineLayout      =           VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout>      descriptorLayouts;

        ~IPipeline() {
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device, pipeline, nullptr);
            }
            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
            }
            for (const auto& layout : descriptorLayouts) {
                if (layout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
                }
            }
        }

    protected:

        explicit IPipeline(const VkDevice& device)
                : m_device(device)
        {}
    };

    class ComputePipeline : public IPipeline {

    public:
        ComputePipeline(const VkDevice& device, const char *scriptPath)
                : IPipeline(device)
        {
            create(scriptPath);
        }

    private:

        void createComputeDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = bindings.size();
            layoutInfo.pBindings = bindings.data();

            VkDescriptorSetLayout computeDescriptorSetLayout;
            if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create compute descriptor set layout!");
            }

            descriptorLayouts.emplace_back(computeDescriptorSetLayout);
        }

        void create(const char *scriptPath) {

            Json script = readJsonFile(scriptPath);
            std::string shaderCode = script["shader-code"];
            auto spirvCode = compileGLSLtoSPIRV(shaderCode, shaderc_compute_shader);
            ShaderModule computeShaderModule(m_device, spirvCode);

            VkPipelineShaderStageCreateInfo computeShaderStageInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = computeShaderModule.module,
                    .pName = "main"
            };

            createComputeDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>{
                    VkDescriptorSetLayoutBinding{
                            .binding = 0,
                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .descriptorCount = 1,
                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                            .pImmutableSamplers = nullptr,
                    }
            });

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size()),
                    .pSetLayouts = descriptorLayouts.data()
            };

            if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create compute pipeline layout!");
            }

            VkComputePipelineCreateInfo pipelineInfo = {
                    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                    .stage = computeShaderStageInfo,
                    .layout = pipelineLayout
            };

            if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
                throw std::runtime_error("failed to create compute pipeline!");
            }
        }
    };

}

#endif //VKHYDROCORE_PIPELINE_H
