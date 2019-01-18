#ifndef VULKAN_APPLICATION_H
#define VULKAN_APPLICATION_H

#include <array>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include "vk_mem_alloc.h"
#include "VulkanCore.h"
#include "Buffer.h"
#include "Terrain.h"
#include "Texture.h"
#include "Camera.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Ensure that GLM works in Vulkan's clip coordinates of 0.0 to 1.0
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#pragma region Constants
const int WIDTH = 800;
const int HEIGHT = 600;
const std::string MODEL_PATH = "models/chalet.obj";
const std::string TEXTURE_PATH = "textures/texture.png";
const VkBool32 WIREFRAME = VK_FALSE;
const VkClearColorValue CLEAR_COLOUR = { 0.7f, 0.3f, 0.25f, 1.0f };
#pragma endregion

#pragma region Frame Buffers
struct VisibilityBuffer
{
	VkFramebuffer frameBuffer;
	vbt::Image visibility, depth;
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

#pragma region Input Functions
		static void ProcessKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void ProcessMouseInput(GLFWwindow* window, int button, int action, int mods);
		void UpdateMouse();
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
		void InitCamera();
		void CreateFrameBuffers();
		void CreateFrameBufferAttachment(VkFormat format, VkImageUsageFlags usage, Image* attachment, VmaAllocator& allocator);
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
		void CreateDepthSampler();
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
		Camera camera;
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
		float frameTime = 0.0f;
#pragma endregion

#pragma region Command Buffer Objects
		std::vector<VkCommandBuffer> visBuffShadeCommandBuffers;
		VkCommandBuffer visBuffWriteCommandBuffer;
		VkCommandPool commandPool;
#pragma endregion

#pragma region Depth Buffer Objects
		Image visBuffShadeDepthImage;
#pragma endregion

#pragma region Buffer Objects
		VmaAllocator allocator;
		Buffer vertexBuffer;
		Buffer indexBuffer;
		Buffer vertexAttributeBuffer;
		Buffer mvpUniformBuffer;
#pragma endregion

#pragma region Texture Objects 
		Texture terrainTexture;
		VkSampler depthSampler;
#pragma endregion

#pragma region Model Objects
		//Mesh chalet;
		Terrain terrain;
#pragma endregion

#pragma region Descriptor Objects
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> shadePassDescriptorSets;
		VkDescriptorSetLayout shadePassDescriptorSetLayout;
		VkDescriptorSet writePassDescriptorSet;
		VkDescriptorSetLayout writePassDescriptorSetLayout;
#pragma endregion

#pragma region Input Objects
		glm::vec2 mousePosition = glm::vec3();
		bool mouseLeftDown = false;
#pragma endregion

#pragma region Debug Objects
		Image debugAttachment;
#pragma endregion
	};
}
#endif 