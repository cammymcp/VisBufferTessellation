#include "VulkanCore.h"
#include "VbtUtils.h"

namespace vbt
{
#pragma region Interface Functions
	void VulkanCore::Init(GLFWwindow* window)
	{
		CreateInstance();
		SetupDebugCallback();
		swapChain.InitSurface(window, instance);
		physicalDevice.Init(instance, swapChain.Surface());
		CreateLogicalDevice();
		swapChain.InitSwapChain(window, physicalDevice.VkHandle(), device);
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

		// Destroy swapchain
		swapChain.CleanUpSwapChain(device);

		// Destroy Logical Device
		vkDestroyDevice(device, nullptr);

		// Destroy debug callback
		if (enableValidationLayers)
			DestroyDebutUtilsMessngerEXT(instance, callback, nullptr);

		// Destroy window surface
		swapChain.CleanUpSurface(instance);

		// Destroy Instance last
		vkDestroyInstance(instance, nullptr);
	}

	void VulkanCore::CreateInstance()
	{
		// Check that required validation layers exist
		if (enableValidationLayers && !CheckValidationLayerSupport())
			throw std::runtime_error("Requested validation layers missing");
	
		// Provide some information about the app (optional)
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Visibility Buffer Tessellation";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;
	
		// Set up instance create info (required)
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
	
		// Get extensions
		auto extensions = GetRequiredInstanceExtensions();
		createInfo.enabledExtensionCount = SCAST_U32(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
	
		// Specify validation layers to enable
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = SCAST_U32(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
			createInfo.enabledLayerCount = 0;
	
		// Create instance
		VkResult instanceResult = vkCreateInstance(&createInfo, nullptr, &instance);
		if (instanceResult != VK_SUCCESS)
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

std::vector<const char*> VulkanCore::GetRequiredInstanceExtensions()
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
	for (uint32_t i = 0; i < glfwExtensionCount; i++)
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
#pragma endregion

#pragma region Device Functions
void VulkanCore::CreateLogicalDevice()
{
	QueueFamilyIndices indices = PhysicalDevice::FindQueueFamilies(physicalDevice.VkHandle(), swapChain.Surface());

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
	deviceFeatures.geometryShader = VK_TRUE;
	deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;

	// Create the logical device
	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = SCAST_U32(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = SCAST_U32(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = SCAST_U32(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
		createInfo.enabledLayerCount = 0;

	// Create the logical device
	if (vkCreateDevice(physicalDevice.VkHandle(), &createInfo, nullptr, &device) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device");
	}

	// Retrieve queue handles
	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &physicalDevice.Queues()->graphics);
	vkGetDeviceQueue(device, indices.presentationFamily.value(), 0, &physicalDevice.Queues()->present);
}
#pragma endregion

#pragma region Presentation Functions
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