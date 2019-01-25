#ifndef IMGUI_H
#define IMGUI_H

#include <vulkan\vulkan.h>
#include <glm\glm.hpp>
#include "vk_mem_alloc.h"
#include "imgui.h"
#include "Image.h"
#include "Buffer.h"

namespace vbt
{
	// Forward declare VulkanApplication class
	class VulkanApplication;

	// Renders on-screen GUI via Dear ImGui library, based on Sascha Willem's implementation
	class ImGUI
	{
	public:
		ImGUI(VulkanApplication* vulkanApp, VmaAllocator* allocator);
		~ImGUI();

		void Init(float width, float height);
		void CreateVulkanResources(PhysicalDevice physDevice, VkCommandPool& cmdPool, VkRenderPass renderPass, VkQueue copyQueue);
		void UpdateFrame();
		void UpdateBuffers();
		void DrawFrame(VkCommandBuffer commandBuffer);
		void CleanUp();

	private:
		struct RenderParamsPushConstant
		{
			glm::vec2 scale;
			glm::vec2 translate;
		} renderParamsPushConstant;
		VkShaderModule CreateShaderModule(const std::vector<char>& code);

		VulkanApplication* vulkanAppHandle;
		VkDevice* deviceHandle;
		VmaAllocator* allocatorHandle;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;
		VkPipelineCache pipelineCache;

		VkDescriptorPool descriptorPool;
		VkDescriptorSet descriptorSet;
		VkDescriptorSetLayout descriptorLayout;
		
		Image fontImage;
		Buffer vertexBuffer;
		Buffer indexBuffer;
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		bool checkboxTest = false;
	};
}

#endif