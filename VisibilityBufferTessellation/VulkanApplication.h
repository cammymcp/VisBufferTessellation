#ifndef VULKAN_APPLICATION_H
#define VULKAN_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <functional>
#include <optional>
#include <set>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
#include "vk_mem_alloc.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#pragma region Constants
const int MAX_FRAMES_IN_FLIGHT = 2;
const int WIDTH = 800;
const int HEIGHT = 600;
const std::string MODEL_PATH = "models/chalet.obj";
const std::string TEXTURE_PATH = "textures/chalet.jpg";
const VkBool32 WIREFRAME = VK_FALSE;
const VkClearColorValue CLEAR_COLOUR = { 0.7f, 0.1f, 0.25f, 1.0f };
#pragma endregion

#pragma region Validation Layers
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

#pragma region Extensions
const std::vector<const char*> deviceExtensions
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
#pragma endregion

#pragma region Queue Families
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

#pragma region Buffer Data
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 colour;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	// Create an attribute description PER ATTRIBUTE (currently pos colour and texcoords)
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, colour);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && colour == other.colour && texCoord == other.texCoord;
	}
};

// Hash calculation for Vertex struct 
namespace std
{
	template<> struct hash<Vertex>
	{
		size_t operator()(Vertex const& vertex) const
		{
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.colour) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}
#pragma endregion

#pragma region Descriptors
struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};
#pragma endregion

class VulkanApplication {
public:
	void Run()
	{
		InitWindow();
		InitVulkan();
		Update();
		CleanUp();
	}

private:

	// Functions

#pragma region Core Functions
	void InitWindow();
	void InitVulkan();
	void Update();
	void CleanUp();
	void CreateInstance();
#pragma endregion

#pragma region Validation Layers and Extensions Functions
	bool CheckValidationLayerSupport();
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	bool CheckForRequiredGlfwExtensions(const char** glfwExtensions, uint32_t glfwExtensionCount);
	std::vector<const char*> GetRequiredExtensions();
#pragma endregion

#pragma region Physical Device Functions
	void SelectPhysicalDevice();
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
#pragma endregion

#pragma region Logical Device Functions
	void CreateLogicalDevice();
#pragma endregion

#pragma region Presentation and Swap Chain Functions
	void CreateSurface();
	void CreateSwapChain();
	void RecreateSwapChain();
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void CreateImageViews();
	void CleanUpSwapChain();
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
#pragma endregion

#pragma region Graphics Pipeline Functions
	void CreateGraphicsPipeline();
	void CreatePipelineLayout();
	void CreateRenderPass();
	VkShaderModule CreateShaderModule(const std::vector<char>& code);
#pragma endregion

#pragma region Drawing Functions
	void CreateFrameBuffers();
	void DrawFrame();
	void CreateSynchronisationObjects();
#pragma endregion

#pragma region Command Buffer Functions
	void CreateCommandPool();
	void AllocateCommandBuffers();
	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
#pragma endregion

#pragma region Depth Buffer Functions
	void CreateDepthResources();
	VkFormat FindDepthFormat();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
#pragma endregion

#pragma region Buffer Functions
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateUniformBuffers();
	void UpdateUniformBuffer(uint32_t currentImage);
	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage allocUsage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VmaAllocation& allocation);
	void CreateVmaAllocator();
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
#pragma endregion

#pragma region Texture Functions
	void CreateTextureImage();
	void CreateTextureImageView();
	void CreateTextureSampler();
	void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& allocation);
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout srcLayout, VkImageLayout dstLayout); 
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
#pragma endregion

#pragma region Model Functions
	void LoadModel();
#pragma endregion

#pragma region Descriptor Functions
	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
#pragma endregion

#pragma region Debug Functions
	void SetupDebugCallback();
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback);
	static void DestroyDebutUtilsMessngerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator);
#pragma endregion

#pragma region Other Functions
	static std::vector<char> ReadFile(const std::string& filename);
	static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
#pragma endregion

	// Objects

#pragma region Core Objects
	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device; // Logical device to interface into physical device
	VkDebugUtilsMessengerEXT callback;
#pragma endregion

#pragma region Presentation/Swap Chain Objects
	VkSurfaceKHR surface; // Interface into window system
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
#pragma endregion

#pragma region Graphics Pipeline Objects
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;
#pragma endregion

#pragma region Drawing Objects
	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences; // Sync objects to prevent CPU submitting too many frames at once
	size_t currentFrame = 0;
	bool framebufferResized = false;
#pragma endregion

#pragma region Command Buffer Objects
	std::vector<VkCommandBuffer> commandBuffers;
	VkCommandPool commandPool;
#pragma endregion

#pragma region Depth Buffer Objects
	VkImage depthImage;
	VmaAllocation depthImageMemory;
	VkImageView depthImageView;
#pragma endregion

#pragma region Buffer Objects
	VmaAllocator allocator;
	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
	std::vector<VkBuffer> uniformBuffers;
	VmaAllocation indexBufferAllocation;
	VmaAllocation vertexBufferAllocation;
	std::vector<VmaAllocation> uniformBufferAllocations;
#pragma endregion

#pragma region Texture Objects 
	VkImage textureImage;
	VmaAllocation textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;
#pragma endregion

#pragma region Model Objects
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
#pragma endregion

#pragma region Descriptor Objects
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
#pragma endregion
};

#endif 