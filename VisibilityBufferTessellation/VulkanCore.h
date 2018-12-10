#ifndef VULKANCORE_H
#define VULKANCORE_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>

#include "VBTTypes.h"
#include "PhysicalDevice.h"

#pragma region Constants
const int MAX_FRAMES_IN_FLIGHT = 1; 
#pragma endregion

#pragma region Required Validation Layers
const std::vector<const char*> validationLayers =
{
	"VK_LAYER_LUNARG_standard_validation",
	"VK_LAYER_RENDERDOC_Capture"
};

#ifdef NDEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = true;
#endif
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
		static VkImageView CreateImageView(const VkDevice &device, VkImage &image, VkFormat format, VkImageAspectFlags aspectFlags);

		// Getters
		VkInstance Instance() const { return instance; }
		PhysicalDevice PhysDevice() const { return physicalDevice; }
		VkDevice Device() const { return device; }
		VkSurfaceKHR Surface() const { return surface; }
		VkSwapchainKHR SwapChain() const { return swapChain; }
		std::vector<VkImage> SwapChainImages() const { return swapChainImages; }
		std::vector<VkImageView> SwapChainImageViews() const { return swapChainImageViews; }
		VkExtent2D SwapChainExtent() const { return swapChainExtent; }
		VkFormat SwapChainImageFormat() const { return swapChainImageFormat; }
		std::vector<VkSemaphore> ImageAvailableSemaphores() const { return imageAvailableSemaphores; }
		std::vector<VkSemaphore> RenderFinishedSemaphores() const { return renderFinishedSemaphores; }
		std::vector<VkFence> Fences() const { return inFlightFences; }

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

		// Device Functions
		void CreateLogicalDevice();

		// Presentation/Swapchain Functions
		void CreateSurface(GLFWwindow* window);
		void CreateSwapChain(GLFWwindow* window);
		void CreateSwapChainImageViews();
		void CreateSynchronisationObjects();
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

		// Core handles
		VkInstance instance;
		VkDebugUtilsMessengerEXT callback;
		PhysicalDevice physicalDevice;
		VkDevice device;
		VkSurfaceKHR surface; // Interface into window system 
		VkSwapchainKHR swapChain;
		VkFormat swapChainImageFormat;
		VkExtent2D swapChainExtent; 
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences; // Sync objects to prevent CPU submitting too many frames at once

		// Core containers
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews;
	};  
} 

#endif // !VULKANCORE_H
