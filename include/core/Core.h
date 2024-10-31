//
// Created by Yucheng Soku on 2024/10/30.
//

#ifndef VKHYDROCORE_CORE_H
#define VKHYDROCORE_CORE_H

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

    class Core {

    public:
        VkInstance                      instance            = VK_NULL_HANDLE;
        VkPhysicalDevice                physicalDevice      = VK_NULL_HANDLE;
        VkDevice                        device              = VK_NULL_HANDLE;
        VkQueue                         computeQueue        = VK_NULL_HANDLE;

    private:
        VkDebugUtilsMessengerEXT        m_debugMessenger    = VK_NULL_HANDLE;

    public:
        Core();
        ~Core();

    private:
        void createInstance();
        void pickPhysicalDevice();
        void setupDebugMessenger();
        void createLogicalDevice();
    };
}
#endif //VKHYDROCORE_CORE_H
