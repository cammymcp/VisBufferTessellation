#ifndef VULKANCORE_H
#define VULKANCORE_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include "VBTTypes.h"
#include "SwapChain.h"

#pragma region Constants
const int MAX_FRAMES_IN_FLIGHT = 1; 
#pragma endregion

#pragma region Validation Layers
const std::vector<const char*> validationLayers =
{
	"VK_LAYER_LUNARG_standard_validation",
	"VK_LAYER_RENDERDOC_Capture"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
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

		// Getters
		VkInstance Instance() const { return instance; }
		PhysicalDevice PhysDevice() const { return physicalDevice; }
		SwapChain Swapchain() const { return swapChain; }
		VkDevice Device() const { return device; }
		VkDevice* DevicePtr() { return &device; }
		std::vector<VkSemaphore> ImageAvailableSemaphores() const { return imageAvailableSemaphores; }
		std::vector<VkSemaphore> RenderFinishedSemaphores() const { return renderFinishedSemaphores; }
		std::vector<VkFence> Fences() const { return inFlightFences; }

	private:
		void CreateInstance();
		void CreateLogicalDevice();
		void CreateSynchronisationObjects();

		// Debug Functions
		void SetupDebugCallback();
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback);
		static void DestroyDebutUtilsMessngerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator);

		// Validation Layers and Extensions Functions
		bool CheckValidationLayerSupport();
		std::vector<const char*> GetRequiredInstanceExtensions();
		bool CheckForRequiredGlfwExtensions(const char** glfwExtensions, uint32_t glfwExtensionCount);

		// Core handles
		VkInstance instance;
		VkDebugUtilsMessengerEXT callback;
		SwapChain swapChain;
		PhysicalDevice physicalDevice;
		VkDevice device;
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences; // Sync objects to prevent CPU submitting too many frames at once
	};  
} 

#endif // !VULKANCORE_H 
