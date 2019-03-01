#include "VbtImGUI.h"
#include "VulkanApplication.h"
#include "VbtUtils.h"

namespace vbt
{	
	void ImGUI::Init(VulkanApplication* app, GLFWwindow* window, ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool)
	{
		// Store app instance
		appHandle = app;

		// Create vulkan resources
		CreateVulkanResources();

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		// Setup Style
		ImGui::StyleColorsDark();

		// Setup bindings to vulkan objects
		ImGui_ImplGlfw_InitForVulkan(window, false);
		ImGui_ImplVulkan_InitInfo initInfo = *info;
		initInfo.DescriptorPool = descriptorPool;
		ImGui_ImplVulkanVbt_Init(&initInfo, renderPass);

		// Load Fonts
		vbt::PhysicalDevice physDevice = appHandle->GetVulkanCore()->PhysDevice();
		VkCommandBuffer fontCmd = BeginSingleTimeCommands(info->Device, commandPool);
		ImGui_ImplVulkan_CreateFontsTexture(fontCmd);
		EndSingleTimeCommands(fontCmd, info->Device, physDevice, commandPool);
		ImGui_ImplVulkan_InvalidateFontUploadObjects();
	}

	void ImGUI::CreateVulkanResources()
	{
		// Descriptor Pool
		std::array<VkDescriptorPoolSize, 1> imGuiPoolSizes = {};
		imGuiPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imGuiPoolSizes[0].descriptorCount = 1;
		VkDescriptorPoolCreateInfo imGuiPoolInfo = {};
		imGuiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		imGuiPoolInfo.poolSizeCount = SCAST_U32(imGuiPoolSizes.size());
		imGuiPoolInfo.pPoolSizes = imGuiPoolSizes.data();
		imGuiPoolInfo.maxSets = SCAST_U32(appHandle->GetVulkanCore()->Swapchain().Images().size());
		if (vkCreateDescriptorPool(appHandle->GetVulkanCore()->Device(), &imGuiPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui descriptor pool");
		}
	}
	
	// Define UI elements to display
	void ImGUI::Update(float frameTime, glm::vec3 cameraPos, glm::vec3 cameraRot)
	{
		// Store local app settings
		static AppSettings currentSettings;
		currentSettings.cameraPos = cameraPos;
		currentSettings.cameraRot.x = WrapAngle(cameraRot.x);
		currentSettings.cameraRot.y = WrapAngle(cameraRot.y);
		currentSettings.cameraRot.z = WrapAngle(cameraRot.z);

		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Update frame time display
		std::rotate(frameTimes.begin(), frameTimes.begin() + 1, frameTimes.end()); // Moves whole collection down one, moving first term to the back to be overwritten
		frameTime *= 1000.0f;
		frameTimes.back() = frameTime;
		if (frameTime < frameTimeMin) {
			frameTimeMin = frameTime;
		}
		if (frameTime > frameTimeMax) {
			frameTimeMax = frameTime;
		}
		char frameTimeChar[64];
		int ret = snprintf(frameTimeChar, sizeof frameTimeChar, "%.2f", frameTime);

		ImGui::Begin("Menu");
		if (ImGui::CollapsingHeader("Measurements"))
		{
			ImGui::Text("Frame Times (ms)");
			ImGui::PlotHistogram("", &frameTimes[0], 50, 0, frameTimeChar, frameTimeMin, frameTimeMax, ImVec2(0, 100));
			if (ImGui::Button("Reset Frame Times", ImVec2(150, 20)))
			{
				ResetFrameGraph();
			}
		}
		if (ImGui::CollapsingHeader("Camera"), ImGuiTreeNodeFlags_DefaultOpen)
		{
			ImGui::Text("Position: "); 
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
			ImGui::SameLine(); if (ImGui::InputFloat("x##pos", &(currentSettings.cameraPos.x), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("y##pos", &(currentSettings.cameraPos.y), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("z##pos", &(currentSettings.cameraPos.z), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::PopItemWidth();
			ImGui::Separator();
			ImGui::Text("Rotation: ");
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
			ImGui::SameLine(); if (ImGui::InputFloat("x##rot", &(currentSettings.cameraRot.x), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("y##rot", &(currentSettings.cameraRot.y), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("z##rot", &(currentSettings.cameraRot.z), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::PopItemWidth();
		}
		if (ImGui::CollapsingHeader("Pipelines"), ImGuiTreeNodeFlags_DefaultOpen)
		{
			ImGui::Text("Switch Pipeline");
			if (ImGui::Button("Visibility Buffer", ImVec2(150, 20)))
			{
				if (currentSettings.pipeline == VB_TESSELLATION)
				{
					currentSettings.pipeline = VISIBILITYBUFFER;
					currentSettings.updateSettings = true;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("VB + Tessellation", ImVec2(150, 20)))
			{
				if (currentSettings.pipeline == VISIBILITYBUFFER)
				{
					currentSettings.pipeline = VB_TESSELLATION;
					currentSettings.updateSettings = true;
				}
			}
			ImGui::Text(currentSettings.pipeline == VISIBILITYBUFFER ? "Current: Visibility Buffer" : "Current: Vis Buff + Tessellation");
		}
		if (ImGui::CollapsingHeader("Render Settings"), ImGuiTreeNodeFlags_DefaultOpen)
		{
			if (ImGui::Checkbox("Show Visibility Buffer", &(currentSettings.showVisBuff))) currentSettings.updateSettings = true;
			if (ImGui::Checkbox("Show Interpolated UV Coords", &(currentSettings.showInterpTex))) currentSettings.updateSettings = true;
			if (currentSettings.pipeline == VB_TESSELLATION) if(ImGui::Checkbox("Show Tess Coords Buffer", &(currentSettings.showTessBuff))) currentSettings.updateSettings = true;
			if (ImGui::Checkbox("Wireframe", &(currentSettings.wireframe))) currentSettings.updateSettings = true;
			if (currentSettings.pipeline == VB_TESSELLATION) if(ImGui::SliderInt("Tess Factor", &(currentSettings.tessellationFactor), 2, 16)) currentSettings.updateSettings = true;
		}
		ImGui::End();
		ImGui::Render();

		// Only update the application when a value has been changed
		if (currentSettings.updateSettings)
		{
			appHandle->ApplySettings(currentSettings);
			currentSettings.updateSettings = false;
		}
	}
	
	void ImGUI::DrawFrame(VkCommandBuffer commandBuffer)
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}

	void ImGUI::ResetFrameGraph()
	{
		frameTimes.fill(0.0f);
		frameTimeMin = 9999.0f;
		frameTimeMax = 0.0f;
	}

	void ImGUI::CleanUp()
	{
		vkDestroyDescriptorPool(appHandle->GetVulkanCore()->Device(), descriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}