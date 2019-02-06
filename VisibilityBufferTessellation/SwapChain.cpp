#include "SwapChain.h"

namespace vbt
{
	void SwapChain::InitSurface(GLFWwindow* window, VkInstance instance)
	{
		CreateSurface(window, instance);
	}

	void SwapChain::InitSwapChain(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		CreateSwapChain(window, physicalDevice, device);
		CreateSwapImageViews(device);
	}

	void SwapChain::CleanUpSwapChain(VkDevice device)
	{
		// Destroy swapchain and its image views
		for (size_t i = 0; i < imageViews.size(); i++) {
			vkDestroyImageView(device, imageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	void SwapChain::CleanUpSurface(VkInstance instance)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

	void SwapChain::RecreateSwapChain(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		// Destroy swapchain and its image views
		for (size_t i = 0; i < imageViews.size(); i++) {
			vkDestroyImageView(device, imageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);

		// Create swapchain again
		CreateSwapChain(window, physicalDevice, device);

		// Create Image Views again
		CreateSwapImageViews(device);
	}


	void SwapChain::CreateSurface(GLFWwindow* window, VkInstance instance)
	{
		// GlfwCreateWindowSurface is platform agnostic and creates the surface object for the relevant platform under the hood
		// The required instance level Windows extensions are included by the glfwGetRequiredExtensions call. 
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface");
		}
	}

	void SwapChain::CreateSwapChain(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);

		// Choose optimal settings from supported details
		VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = ChoosePresentMode(swapChainSupport.presentModes);
		VkExtent2D swapExtent = ChooseExtent(swapChainSupport.capabilities, window);

		// Set queue length
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		// Set up create info
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = swapExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		// Define how swap images are shared between queue families
		QueueFamilyIndices indices = PhysicalDevice::FindQueueFamilies(physicalDevice, surface);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentationFamily.value() };
		if (indices.graphicsFamily != indices.presentationFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; // Maybe disable this later on for more consistent test results
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// Create swap chain
		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain");
		}

		// Retrieve swap chain image handles
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		images.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());

		// Store image format and resolution
		imageFormat = surfaceFormat.format;
		extent = swapExtent;
	}

	void SwapChain::CreateSwapImageViews(VkDevice device)
	{
		imageViews.resize(images.size());

		for (uint32_t i = 0; i < images.size(); i++)
		{
			imageViews[i] = CreateImageView(device, images[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	SwapChainSupportDetails SwapChain::QuerySwapChainSupport(VkPhysicalDevice device)
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


	VkSurfaceFormatKHR SwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		// Ideally the surface doesn't prefer any one format, so we can choose our own
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		// Otherwise we'll check for our preferred combination
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		// If that fails, it's usually ok to settle with the first specified format. Alternatively could rank them
		return availableFormats[0];
	}

	// Present modes supported by Vulkan: 
	// Immediate (tearing likely) 
	// Fifo (V-sync) 
	// Fifo relaxed (Doesn't wait for next v-blank if app is late. Tearing possible)
	// Mailbox (V-sync that replaces queued images when full. Can be used for triple buffering)
	VkPresentModeKHR SwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
	{
		//// Only Fifo is guaranteed to be available, so use that as a default
		//VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
		//
		//for (const auto& availablePresentMode : availablePresentModes)
		//{
		//	// Triple buffering is nice, lets try for Mailbox first
		//	if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		//	{
		//		return availablePresentMode;
		//	}
		//	// Prefer immediate over Fifo, since some drivers unfortunately dont support it
		//	else if(availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		//	{
		//		bestMode = availablePresentMode;
		//	}
		//}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	// Swap extent is the resolution of the swap chain images. This is *almost* always exactly equal to 
	// the target window size
	VkExtent2D SwapChain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
	{
		// Vulkan wants us to just return currentExtent, to match the window size. However some window managers
		// allow us to differ, and let us know by setting currentExtent to the max value of uint32_t. We obviously
		// have to check for that before returning currentExtent
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			// Just return current extent (the exact size of the window)
			return capabilities.currentExtent;
		}
		else
		{
			// Current extent is useless. Return the window size, making sure its within the min/max range of the capabilities
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent = {
				SCAST_U32(width),
				SCAST_U32(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	VkImageView SwapChain::CreateImageView(const VkDevice &device, VkImage &image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create texture image view");
		}

		return imageView;
	}
}