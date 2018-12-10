#ifndef PHYSICALDEVICE_H
#define PHYSICALDEVICE_H

#include <vulkan\vulkan.h>
#include <functional>
#include <optional>
#include <set>
#include "VBTTypes.h"

#pragma region Required Extensions
const std::vector<const char*> deviceExtensions
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	"VK_KHR_shader_draw_parameters"
};
#pragma endregion

namespace vbt
{
	class PhysicalDevice
	{
	public:
		void Init(VkInstance instance, VkSurfaceKHR surface);
		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

		VkPhysicalDevice Device() const { return physicalDevice; }
		DeviceQueues* Queues() { return &queues; }
		const std::vector<const char*> Extensions() { return deviceExtensions; }

	private:
		void SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
		bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		DeviceQueues queues;
	};
}

#endif // !PHYSICALDEVICE_H
