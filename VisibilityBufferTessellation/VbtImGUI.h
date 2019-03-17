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
	enum PipelineType { VISIBILITYBUFFER, VB_TESSELLATION };
	struct AppSettings
	{
		glm::vec3 cameraPos;
		glm::vec3 cameraRot;
		glm::vec3 lightDirection;
		glm::vec4 lightDiffuse;
		glm::vec4 lightAmbient;
		PipelineType pipeline;
		int tessellationFactor = 34;
		bool showVisBuff = false;
		bool showTessBuff = false;
		bool showInterpTex = false;
		bool wireframe = false;
		bool updateSettings = false; // When true this class will call the UpdateSettings function of the appHandle
	};

	// Renders on-screen GUI via Dear ImGui library
	class ImGUI
	{
	public:
		void Init(VulkanApplication* app, GLFWwindow* window, ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool, int visBuffTriCount, int tessTriCount);
		void Recreate(ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool);
		void CreateVulkanResources();
		void Update(float frameTime, glm::vec3 cameraPos, glm::vec3 cameraRot, glm::vec3 lightDirection, glm::vec4 lightDiffuse, glm::vec4 lightAmbient);
		void DrawFrame(VkCommandBuffer commandBuffer);
		void CleanUp();

	private:
		void ResetFrameGraph();

		// Vulkan Resources
		VkDescriptorPool descriptorPool;

		VulkanApplication* appHandle;
		std::array<float, 50> frameTimes{};
		float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
		int visBuffTriCount = 0, tessTricount = 0;
	};
}

#endif