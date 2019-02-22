#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include <vulkan\vulkan.h>
#include <GLFW\glfw3.h>
#include "VBTTypes.h"
#include "VbtUtils.h"
#include <algorithm>
#include "PhysicalDevice.h"

namespace vbt
{
	class SwapChain
	{
	public:
		void InitSurface(GLFWwindow* window, VkInstance instance);
		void InitSwapChain(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkDevice device);
		void CleanUpSwapChain(VkDevice device);
		void CleanUpSurface(VkInstance instance);
		static VkImageView CreateImageView(const VkDevice &device, VkImage &image, VkFormat format, VkImageAspectFlags aspectFlags);

		VkSwapchainKHR VkHandle() const { return swapChain; }
		VkSurfaceKHR Surface() const { return surface; }
		VkExtent2D Extent() const { return extent; }
		VkFormat ImageFormat() const { return imageFormat; }
		std::vector<VkImage> Images() const { return images; }
		std::vector<VkImageView> ImageViews() const { return imageViews; }

	private:
		void CreateSwapChain(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkDevice device);
		void CreateSurface(GLFWwindow* window, VkInstance instance);
		void CreateSwapImageViews(VkDevice device);
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
		VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

		VkSwapchainKHR swapChain;
		VkSurfaceKHR surface; // Interface into window system 
		VkFormat imageFormat;
		VkExtent2D extent;

		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
	};
}

#endif // !SWAPCHAIN_H
