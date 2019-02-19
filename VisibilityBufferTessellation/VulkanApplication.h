#ifndef VULKAN_APPLICATION_H
#define VULKAN_APPLICATION_H

#include <array>
#include <cstdlib>
#include <chrono>
#include "vk_mem_alloc.h"
#include "VulkanCore.h"
#include "Buffer.h"
#include "Terrain.h"
#include "Texture.h"
#include "Camera.h"
#include "VbtImGUI.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Ensure that GLM works in Vulkan's clip coordinates of 0.0 to 1.0
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#pragma region Constants
const int WIDTH = 1280;
const int HEIGHT = 720;
const std::string MODEL_PATH = "models/chalet.obj";
#pragma endregion

#pragma region Frame Buffers
struct VisibilityBuffer
{
	vbt::Image visibility;
};

struct TessellationVisibilityBuffer
{
	vbt::Image visibility, tessCoords;
};
#pragma endregion

#pragma region Uniform Buffers
struct MVPUniformBufferObject 
{
	glm::mat4 mvp;
	glm::mat4 proj;
};

struct TessFactorUBO
{
	float tessellationFactor;
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

		VulkanCore* GetVulkanCore() { return vulkan; }
		void ApplySettings(AppSettings settings);

		const std::string title = "Visibility Buffer Tessellation";
	private:
		// Functions
#pragma region Core Functions
		void InitWindow();
		void Init();
		void Update();
		void CleanUp();
#pragma endregion

#pragma region ImGui Functions
		void InitImGui(VkRenderPass renderPass);
#pragma endregion

#pragma region Input Functions
		static void ProcessKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void ProcessMouseInput(GLFWwindow* window, int button, int action, int mods);
		void UpdateMouse();
#pragma endregion

#pragma region Presentation and Swap Chain Functions
		void RecreateSwapChain();
		void CleanUpSwapChainResources();
#pragma endregion

#pragma region Graphics Pipeline Functions
		void CreatePipelineCache();
		void CreateShadePipelines();
		void CreateWritePipelines();
		void CreateVisBuffShadePipelineLayout();
		void CreateVisBuffWritePipelineLayout();
		void CreateTessShadePipelineLayout();
		void CreateTessWritePipelineLayout();
		void CreateRenderPasses();
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
		void AllocateCommandBuffers();
		void RecordCommandBuffers();
#pragma endregion

#pragma region Depth Buffer Functions
		void CreateDepthResources();
		VkFormat FindDepthFormat();
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
#pragma endregion

#pragma region Buffer Functions
		void CreateUniformBuffers();
		void UpdateUniformBuffers();
		void CreateVmaAllocator();
#pragma endregion

#pragma region Descriptor Functions
		void CreateDescriptorPool();
		void CreateShadePassDescriptorSetLayouts();
		void CreateShadePassDescriptorSets();
		void CreateVisBuffWritePassDescriptorSetLayout();
		void CreateWritePassDescriptorSet();
		void CreateTessWritePassDescriptorSetLayout();
		void CreateTessWritePassDescriptorSet();
#pragma endregion

#pragma region Other Functions
		static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
#pragma endregion

#pragma region Shared Objects
		GLFWwindow* window;
		VulkanCore* vulkan;
		ImGUI imGui;
		Camera camera;
		VkPipelineCache pipelineCache;
		VkCommandPool commandPool;
		VkDescriptorPool descriptorPool;
		VmaAllocator allocator;
		std::vector<VkCommandBuffer> commandBuffers;
		vbt::Image depthImage;
#pragma endregion

#pragma region Visibility Buffer Pipeline 
		VisibilityBuffer visibilityBuffer;
		VkRenderPass visBuffRenderPass;
		VkPipeline visBuffShadePipeline;
		VkPipeline visBuffWritePipeline;
		VkPipelineLayout visBuffShadePipelineLayout;
		VkPipelineLayout visBuffWritePipelineLayout;
		std::vector<VkFramebuffer> visBuffFramebuffers;
		VkDescriptorSet visBuffWritePassDescSet;
		VkDescriptorSetLayout visBuffWritePassDescSetLayout;
		std::vector<VkDescriptorSet> visBuffShadePassDescSets;
		VkDescriptorSetLayout visBuffShadePassDescSetLayout;
#pragma endregion

#pragma region Visibility Buffer + Tessellation Pipeline
		TessellationVisibilityBuffer tessVisibilityBuffer;
		VkRenderPass tessRenderPass;
		VkPipeline tessShadePipeline;
		VkPipeline tessWritePipeline;
		VkPipelineLayout tessShadePipelineLayout;
		VkPipelineLayout tessWritePipelineLayout;
		std::vector<VkFramebuffer> tessFramebuffers;
		VkDescriptorSet tessWritePassDescSet;
		VkDescriptorSetLayout tessWritePassDescSetLayout;
		std::vector<VkDescriptorSet> tessShadePassDescSets;
		VkDescriptorSetLayout tessShadePassDescSetLayout;
#pragma endregion

#pragma region Geometry
		Terrain terrain;
		Buffer mvpUniformBuffer;
		Buffer tessFactorBuffer;
#pragma endregion

#pragma region Input, Settings, Counters and Flags
		PipelineType currentPipeline = VISIBILITYBUFFER;
		size_t currentFrame = 0;
		bool framebufferResized = false;
		float frameTime = 0.0f;
		glm::vec2 mousePosition = glm::vec3();
		bool mouseLeftDown = false;
		bool mouseRightDown = false;
#pragma endregion

#pragma region Debug Objects
		Image debugAttachment;
#pragma endregion
	};
}
#endif 