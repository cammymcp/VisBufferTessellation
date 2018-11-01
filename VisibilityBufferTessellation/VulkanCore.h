#ifndef VULKANCORE_H
#define VULKANCORE_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan\vulkan.h>
#include <functional>
#include <set>
#include <optional>
#include <algorithm>
#include <iostream>

#pragma region Required Validation Layers
const std::vector<const char*> validationLayers =
{
	"VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
#pragma endregion

#pragma region Required Extensions
const std::vector<const char*> deviceExtensions
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
#pragma endregion

#pragma region Device Queues
struct DeviceQueues
{
	VkQueue graphics;
	VkQueue present;
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentationFamily;

	bool isSuitable()
	{
		return graphicsFamily.has_value() && presentationFamily.has_value();
	}
};
#pragma endregion

#pragma region Swap Chain Support
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
#pragma endregion

namespace vbt
{
	class VulkanCore
	{
	public:
		// Interface Functions
		void Init(GLFWwindow* window);
		void CleanUp(); 
		void RecreateSwapChain(GLFWwindow* window);
		static VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

		// Getters
		VkInstance Instance() const { return instance; }
		VkPhysicalDevice PhysicalDevice() const { return physicalDevice; }
		VkDevice Device() const { return device; }
		VkSurfaceKHR Surface() const { return surface; }
		VkSwapchainKHR SwapChain() const { return swapChain; }
		std::vector<VkImage> SwapChainImages() const { return swapChainImages; }
		std::vector<VkImageView> SwapChainImageViews() const { return swapChainImageViews; }
		VkExtent2D SwapChainExtent() const { return swapChainExtent; }
		VkFormat SwapChainImageFormat() const { return swapChainImageFormat; }
		DeviceQueues Queues() const { return queues; }

	private:
		// Instance Functions
		void CreateInstance();

		// Debug Functions
		void SetupDebugCallback();
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback);
		static void DestroyDebutUtilsMessngerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator);

		// Validation Layers and Extensions Functions
		bool CheckValidationLayerSupport();
		std::vector<const char*> GetRequiredExtensions();
		bool CheckForRequiredGlfwExtensions(const char** glfwExtensions, uint32_t glfwExtensionCount);
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

		// Device Functions
		void SelectPhysicalDevice();
		bool isDeviceSuitable(VkPhysicalDevice device);
		void CreateLogicalDevice();

		// Presentation/Swapchain Functions
		void CreateSurface(GLFWwindow* window);
		void CreateSwapChain(GLFWwindow* window);
		void CreateSwapChainImageViews();
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

		// Core handles
		VkInstance instance;
		VkDebugUtilsMessengerEXT callback;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device;
		DeviceQueues queues;
		VkSurfaceKHR surface; // Interface into window system 
		VkSwapchainKHR swapChain;
		VkFormat swapChainImageFormat;
		VkExtent2D swapChainExtent;

		// Core containers
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews;
	};  
}

#endif // !VULKANCORE_H
