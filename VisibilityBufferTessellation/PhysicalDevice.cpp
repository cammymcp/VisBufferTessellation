#include "PhysicalDevice.h"

namespace vbt
{
	void PhysicalDevice::Init(VkInstance instance, VkSurfaceKHR surface)
	{
		SelectPhysicalDevice(instance, surface);
	}

	void PhysicalDevice::SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
	{
		// How many devices are discoverable?
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			// Uh oh
			throw std::runtime_error("Failed to find GPUs with Vulkan Support");
		}

		// Get available devices
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		// Are any of the devices suitable for our needs?
		for (const auto& device : devices) {
			if (isDeviceSuitable(device, surface)) {
				physicalDevice = device;
				break;
			}
		}
		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("Failed to find a suitable GPU");
		}
	}

	bool PhysicalDevice::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		// Get properties and features of graphics device
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		// Check for device features
		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		// We only want dedicated graphics cards
		bool discrete = (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

		// Check for required queue families
		QueueFamilyIndices indices = FindQueueFamilies(device, surface);
		// Check for extension support
		bool extensionsSupported = CheckDeviceExtensionSupport(device);
		// Check for an adequate swap chain support
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device, surface);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isSuitable() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy && discrete && supportedFeatures.geometryShader && supportedFeatures.fragmentStoresAndAtomics && supportedFeatures.tessellationShader;
	}

	bool PhysicalDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		// Get all supported device extensions
		uint32_t supportedExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, nullptr);
		std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, supportedExtensions.data());

		// Check for the ones we want
		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto& extension : supportedExtensions)
		{
			// If the supported extension name is found in the required set, erase it
			requiredExtensions.erase(extension.extensionName);
		}

		// If requiredExtensions is empty then all of them are supported
		return requiredExtensions.empty();
	}


	QueueFamilyIndices PhysicalDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices;

		// Get supported queue families from physical device
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

		// Check each family for required support. Note: it's likely that graphics and presentation are supported
		// by the same queue family for most graphics devices. But just in case, check for each as seperate families.
		// Could add some logic to prefer a device that supports both in one family for better performance.
		int i = 0;
		for (const auto& queueFamily : queueFamilyProperties)
		{
			// Check for graphics support
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}
			// Check for presentation support 
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (queueFamilyCount > 0 && presentSupport)
			{
				indices.presentationFamily = i;
			}

			if (indices.isSuitable())
			{
				break;
			}

			i++;
		}

		return indices;
	}

	SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;

		// Get basic surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		// Get supported surface formats
		uint32_t supportedFormatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &supportedFormatCount, nullptr);
		if (supportedFormatCount > 0)
		{
			details.formats.resize(supportedFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &supportedFormatCount, details.formats.data());
		}

		// Get supported presentation modes
		uint32_t supportedPresentModes = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &supportedPresentModes, nullptr);
		if (supportedPresentModes > 0)
		{
			details.presentModes.resize(supportedPresentModes);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &supportedPresentModes, details.presentModes.data());
		}

		return details;
	}

}