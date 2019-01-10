#ifndef VULKAN_APPLICATION_H
#define VULKAN_APPLICATION_H

#include <array>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include "vk_mem_alloc.h"
#include "VulkanCore.h"
#include "Buffer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Ensure that GLM works in Vulkan's clip coordinates of 0.0 to 1.0
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#pragma region Constants
const int WIDTH = 800;
const int HEIGHT = 600;
const std::string MODEL_PATH = "models/chalet.obj";
const std::string TEXTURE_PATH = "textures/chalet.jpg";
const VkBool32 WIREFRAME = VK_FALSE;
const VkClearColorValue CLEAR_COLOUR = { 0.7f, 0.3f, 0.25f, 1.0f };
#pragma endregion

#pragma region Frame Buffers
struct FrameBufferAttachment
{
	VkImage image;
	VkImageView imageView;
	VmaAllocation imageMemory;
	VkFormat format;
};
struct VisibilityBuffer
{
	VkFramebuffer frameBuffer;
	FrameBufferAttachment visibility, depth;
};
#pragma endregion

#pragma region Buffer Data
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 colour;
	glm::vec2 uv;

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
		attributeDescriptions[2].offset = offsetof(Vertex, uv);

		return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && colour == other.colour && uv == other.uv;
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
				(hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}

// 16-byte aligned vertex attributes. 
struct VertexAttributes
{
	glm::vec4 posXYZcolX;
	glm::vec4 colYZtexXY;
};
#pragma endregion

#pragma region Uniform Buffers
struct UniformBufferObject
{
	glm::mat4 mvp;
	glm::mat4 proj;
	//glm::mat4 view;
	//glm::mat4 invViewProj;
};
#pragma endregion

namespace vbt
{
	class VulkanApplication {
	public:
		void Run()
		{
			InitWindow();
			Init();
			Update();
			CleanUp();
		}

	private:

		// Functions

#pragma region Core Functions
		void InitWindow();
		void Init();
		void Update();
		void CleanUp();
#pragma endregion

#pragma region Presentation and Swap Chain Functions
		void RecreateSwapChain();
		void CleanUpSwapChain();
#pragma endregion

#pragma region Graphics Pipeline Functions
		void CreatePipelineCache();
		void CreateVisBuffShadePipeline();
		void CreateVisBuffWritePipeline();
		void CreateVisBuffShadePipelineLayout();
		void CreateVisBuffWritePipelineLayout();
		void CreateVisBuffWriteRenderPass();
		void CreateVisBuffShadeRenderPass();
		VkShaderModule CreateShaderModule(const std::vector<char>& code);
#pragma endregion

#pragma region Drawing Functions
		void CreateFrameBuffers();
		void CreateFrameBufferAttachment(VkFormat format, VkImageUsageFlags usage, FrameBufferAttachment* attachment);
		void DrawFrame();
#pragma endregion

#pragma region Command Buffer Functions
		void CreateCommandPool();
		void RecordVisBuffShadeCommandBuffers();
		void RecordVisBuffWriteCommandBuffer();
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
		void CreateAttributeBuffer();
		void CreateIndexBuffer();
		void CreateUniformBuffers();
		void UpdateMVPUniformBuffer();
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage allocUsage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VmaAllocation& allocation);
		void CreateVmaAllocator();
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
#pragma endregion

#pragma region Texture Functions
		void CreateTextureImage();
		void CreateTextureImageView();
		void CreateTextureSampler();
		void CreateDepthSampler();
		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& allocation);
		void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout srcLayout, VkImageLayout dstLayout);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
#pragma endregion

#pragma region Model Functions
		void LoadModel();
#pragma endregion

#pragma region Descriptor Functions
		void CreateDescriptorPool();
		void CreateShadePassDescriptorSetLayout();
		void CreateWritePassDescriptorSetLayout();
		void CreateShadePassDescriptorSets(); 
		void CreateWritePassDescriptorSet();
#pragma endregion

#pragma region Other Functions
		static std::vector<char> ReadFile(const std::string& filename);
		static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
#pragma endregion

		// Handles and Containers
#pragma region Core Objects
		GLFWwindow* window;
		VulkanCore* vulkan;
#pragma endregion

#pragma region Graphics Pipeline Objects
		VkPipeline visBuffShadePipeline;
		VkPipeline visBuffWritePipeline;
		VkPipelineCache pipelineCache;
		VkPipelineLayout visBuffShadePipelineLayout;
		VkPipelineLayout visBuffWritePipelineLayout;
		VkRenderPass visBuffWriteRenderPass;
		VkRenderPass visBuffShadeRenderPass;
#pragma endregion

#pragma region Drawing Objects
		std::vector<VkFramebuffer> swapChainFramebuffers;
		VisibilityBuffer visibilityBuffer;
		VkSemaphore visBuffWriteSemaphore;
		size_t currentFrame = 0;
		bool framebufferResized = false;
#pragma endregion

#pragma region Command Buffer Objects
		std::vector<VkCommandBuffer> visBuffShadeCommandBuffers;
		VkCommandBuffer visBuffWriteCommandBuffer;
		VkCommandPool commandPool;
#pragma endregion

#pragma region Depth Buffer Objects
		VkImage visBuffShadeDepthImage;
		VmaAllocation visBuffShadeDepthImageMemory;
		VkImageView visBuffShadeDepthImageView;
#pragma endregion

#pragma region Buffer Objects
		VmaAllocator allocator;
		Buffer vertexBuffer;
		Buffer indexBuffer;
		Buffer vertexAttributeBuffer;
		Buffer mvpUniformBuffer;
#pragma endregion

#pragma region Texture Objects 
		VkImage textureImage;
		VmaAllocation textureImageMemory;
		VkImageView textureImageView;
		VkSampler textureSampler;
		VkSampler depthSampler;
#pragma endregion

#pragma region Model Objects
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<VertexAttributes> vertexAttributeData;
#pragma endregion

#pragma region Descriptor Objects
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> shadePassDescriptorSets;
		VkDescriptorSetLayout shadePassDescriptorSetLayout;
		VkDescriptorSet writePassDescriptorSet;
		VkDescriptorSetLayout writePassDescriptorSetLayout;
#pragma endregion

#pragma region Debug Objects
		FrameBufferAttachment debugAttachment;
#pragma endregion
	};
}
#endif 