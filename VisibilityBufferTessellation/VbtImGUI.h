#ifndef IMGUI_H
#define IMGUI_H

#include <array>

#include "vulkan\vulkan.h"
#include "vk_mem_alloc.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "PhysicalDevice.h"

namespace vbt
{
	// Forward declarations
	class VulkanApplication;

	// Renders on-screen GUI via Dear ImGui library
	class ImGUI
	{
	public:
		void Init(VulkanApplication* app, GLFWwindow* window, ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool);
		void CreateVulkanResources(VulkanApplication* app);
		void UpdateFrame(float frameTime);
		void DrawFrame(VkCommandBuffer commandBuffer);
		//void AllocateCommandBuffers(VkDevice device, VkCommandPool commandPool, size_t numBuffers);
		//void UpdateCommandBuffer(int index);
		void CleanUp();

		//std::vector<VkCommandBuffer> CommandBuffers() const { return commandBuffers; }
		//std::vector<VkSemaphore> Semaphores() const { return semaphores; }
	private:
		void ResetFrameGraph();

		// Vulkan Objects
		VkDescriptorPool descriptorPool;
		//std::vector<VkCommandBuffer> commandBuffers;
		//std::vector<VkSemaphore> semaphores;

		bool checkboxTest = false;
		std::array<float, 50> frameTimes{};
		float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
	};
}

#endif