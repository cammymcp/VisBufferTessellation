#ifndef IMGUI_H
#define IMGUI_H

#include <array>

#include "vulkan\vulkan.h"
#include "vk_mem_alloc.h"
#include <glm/glm.hpp>
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "PhysicalDevice.h"

namespace vbt
{
	// Forward declarations
	class VulkanApplication;

	// ImGui Settings
	struct AppSettings
	{
		glm::vec3 cameraPos;
		glm::vec3 cameraRot;
		bool updateSettings = false; // When true this class will call the UpdateSettings function of the appHandle
	};

	// Renders on-screen GUI via Dear ImGui library
	class ImGUI
	{
	public:
		void Init(VulkanApplication* app, GLFWwindow* window, ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool);
		void CreateVulkanResources();
		void Update(float frameTime, glm::vec3 cameraPos, glm::vec3 cameraRot);
		void DrawFrame(VkCommandBuffer commandBuffer);
		//void AllocateCommandBuffers(VkDevice device, VkCommandPool commandPool, size_t numBuffers);
		//void UpdateCommandBuffer(int index);
		void CleanUp();

		//std::vector<VkCommandBuffer> CommandBuffers() const { return commandBuffers; }
		//std::vector<VkSemaphore> Semaphores() const { return semaphores; }
	private:
		void ResetFrameGraph();

		VulkanApplication* appHandle;

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