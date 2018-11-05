#include "VulkanCore.h"

namespace vbt
{
#pragma region Interface Functions
	void VulkanCore::Init(GLFWwindow* window)
	{
		CreateInstance();
		SetupDebugCallback();
		CreateSurface(window);
		SelectPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain(window);
		CreateSwapChainImageViews();
		CreateSynchronisationObjects();
	}

	void VulkanCore::CleanUp()
	{
		// Destroy Sync Objects
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}

		// Destroy swapchain and its image views
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vkDestroyImageView(device, swapChainImageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);

		// Destroy Logical Device
		vkDestroyDevice(device, nullptr);

		// Destroy debug callback
		if (enableValidationLayers)
			DestroyDebutUtilsMessngerEXT(instance, callback, nullptr);

		// Destroy window surface
		vkDestroySurfaceKHR(instance, surface, nullptr);

		// Destroy Instance last
		vkDestroyInstance(instance, nullptr);
	}

	void VulkanCore::RecreateSwapChain(GLFWwindow* window)
	{
		// Destroy swapchain and its image views
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vkDestroyImageView(device, swapChainImageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);

		// Create swapchain again
		CreateSwapChain(window);

		// Create Image Views again
		CreateSwapChainImageViews();
	}

	void VulkanCore::CreateInstance()
	{
		// Check that required validation layers exist
		if (enableValidationLayers && !CheckValidationLayerSupport())
			throw std::runtime_error("Requested validation layers missing");
	
		// Provide some information about the app (optional)
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Test";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;
	
		// Set up instance create info (required)
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
	
		// Get extensions
		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
	
		// Specify validation layers to enable
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
			createInfo.enabledLayerCount = 0;
	
		// Create instance
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create instance");
		}
	}
#pragma endregion
	
#pragma region Validation Layers and Extensions
//Checks if the validation layers specified in the header file are available for loading
bool VulkanCore::CheckValidationLayerSupport()
{
	// Get list of all available layers
	uint32_t supportedLayerCount;
	vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);
	std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
	vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());

	// Now check if the layers we want exist in supported layers
	std::set<std::string> requiredLayers(validationLayers.begin(), validationLayers.end());
	for (const auto& layer : supportedLayers)
	{
		requiredLayers.erase(layer.layerName);
	}

	// If requiredLayers is empty then all of them are supported
	return requiredLayers.empty();
}

std::vector<const char*> VulkanCore::GetRequiredExtensions()
{
	// Get extensions required by glfw
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Check that they exist
	if (!CheckForRequiredGlfwExtensions(glfwExtensions, glfwExtensionCount))
		throw std::runtime_error("GLFW Required Extension Missing");

	// Create extensions list
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	// Add any more we need (existence is implied by the availability of validation layers)
	if (enableValidationLayers)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

bool VulkanCore::CheckForRequiredGlfwExtensions(const char** glfwExtensions, uint32_t glfwExtensionCount)
{
	// Find all supported extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	// For each required extensions
	for (int i = 0; i < glfwExtensionCount; i++)
	{
		bool requiredExtensionFound = false;

		// Check each supported extension
		for (const auto& extension : extensions)
		{
			if (strcmp(extension.extensionName, glfwExtensions[i]) == 0)
			{
				requiredExtensionFound = true;
				break;
			}
		}

		if (!requiredExtensionFound)
		{
			return false;
		}
	}

	return true;
}

bool VulkanCore::CheckDeviceExtensionSupport(VkPhysicalDevice device)
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
#pragma endregion

#pragma region Device Functions
void VulkanCore::SelectPhysicalDevice()
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
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}
	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to find a suitable GPU");
	}
}

bool VulkanCore::isDeviceSuitable(VkPhysicalDevice device)
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
	QueueFamilyIndices indices = FindQueueFamilies(device);
	// Check for extension support
	bool extensionsSupported = CheckDeviceExtensionSupport(device);
	// Check for an adequate swap chain support
	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	return indices.isSuitable() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy && discrete;
}

QueueFamilyIndices VulkanCore::FindQueueFamilies(VkPhysicalDevice device)
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

void VulkanCore::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);

	// Create a DeviceQueueCreateInfo for each required queue family
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentationFamily.value() };

	// Set priority for command buffer execution scheduling
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Define required device features
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	// Create the logical device
	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
		createInfo.enabledLayerCount = 0;

	// Create the logical device
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device");
	}

	// Retrieve queue handles
	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &queues.graphics);
	vkGetDeviceQueue(device, indices.presentationFamily.value(), 0, &queues.present);
}
#pragma endregion

#pragma region Presentation/SwapChain Functions
void VulkanCore::CreateSurface(GLFWwindow* window)
{
	// GlfwCreateWindowSurface is platform agnostic and creates the surface object for the relevant platform under the hood
	// The required instance level Windows extensions are included by the glfwGetRequiredExtensions call. 
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface");
	}
}

void VulkanCore::CreateSwapChain(GLFWwindow* window)
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);

	// Choose optimal settings from supported details
	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, window);

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
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	// Define how swap images are shared between queue families
	QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);
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
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	// Store image format and resolution
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void VulkanCore::CreateSwapChainImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());

	for (uint32_t i = 0; i < swapChainImages.size(); i++)
	{
		swapChainImageViews[i] = CreateImageView(device, swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

// We'll need to set up semaphores to ensure the order of the asynchronous functions
void VulkanCore::CreateSynchronisationObjects()
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Signal the fence so that the first frame is rendered

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{

			throw std::runtime_error("Failed to create syncrhonisation objects for a frame");
		}
	}
}

VkImageView VulkanCore::CreateImageView(const VkDevice &device, VkImage &image, VkFormat format, VkImageAspectFlags aspectFlags)
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

SwapChainSupportDetails VulkanCore::QuerySwapChainSupport(VkPhysicalDevice device)
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


VkSurfaceFormatKHR VulkanCore::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
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
VkPresentModeKHR VulkanCore::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
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
VkExtent2D VulkanCore::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
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
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}
#pragma endregion

#pragma region Debug Functions
void VulkanCore::SetupDebugCallback()
{
	// If we're not using validation layers then we don't need a callback
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData = nullptr; // Optional

									// Set up object from external function
	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS)
		throw std::runtime_error("Failed to setup debug callback!");
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanCore::DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::string messageTypeStr = "";
	switch (messageType)
	{
	case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
	{
		messageTypeStr = "General";
		break;
	}
	case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
	{
		messageTypeStr = "Violation";
		break;
	}
	case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
	{
		messageTypeStr = "Performance";
		break;
	}
	}
	std::cerr << "Validation Layer Message (" << messageTypeStr.c_str() << "): " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

// Gets the external function from extension to set up the messenger object
VkResult VulkanCore::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

// Loads external function for destroying messenger object and executes it
void VulkanCore::DestroyDebutUtilsMessngerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}
#pragma endregion

} // Namespace