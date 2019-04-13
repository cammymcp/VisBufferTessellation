#include "VbtImGUI.h"
#include "VulkanApplication.h"
#include "VbtUtils.h"

namespace vbt
{	
	void ImGUI::Init(VulkanApplication* app, GLFWwindow* window, ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool, int visBuffTriCount, int tessTriCount)
	{
		// Store app instance and triangle counts
		appHandle = app;
		this->visBuffTriCount = visBuffTriCount;
		this->tessTricount = tessTriCount; // Tri-count in host memory won't change, subdivision is calculated locally per-frame

		// Create vulkan resources
		CreateVulkanResources();

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		// Setup Style
		ImGui::StyleColorsDark();

		// Setup bindings to vulkan objects
		ImGui_ImplGlfw_InitForVulkan(window, true);
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

	// Reinitialises ImGui for new render pass configuration
	void ImGUI::Recreate(ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool)
	{
		vkDestroyDescriptorPool(appHandle->GetVulkanCore()->Device(), descriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();

		CreateVulkanResources();

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
	void ImGUI::Update(double frameTime, double forwardTime, double deferredTime, glm::vec3 cameraPos, glm::vec3 cameraRot, glm::vec3 lightDirection, glm::vec4 lightDiffuse, glm::vec4 lightAmbient)
	{
		// Store local app settings
		static AppSettings currentSettings;
		currentSettings.cameraPos = cameraPos;
		currentSettings.cameraRot.x = WrapAngle(cameraRot.x);
		currentSettings.cameraRot.y = WrapAngle(cameraRot.y);
		currentSettings.cameraRot.z = WrapAngle(cameraRot.z);
		currentSettings.lightDiffuse = lightDiffuse;
		currentSettings.lightDirection = lightDirection;
		currentSettings.lightAmbient = lightAmbient;

		// Calculate tessellation tri-count at current tess factor
		int tessCount = tessTricount * CalculateTriangleSubdivision(currentSettings.tessellationFactor);

		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Update pass times
		std::rotate(frameTimes.begin(), frameTimes.begin() + 1, frameTimes.end()); // Moves whole collection down one, moving first term to the back to be overwritten
		frameTime *= 1000.0;
		frameTimes.back() = (float)frameTime;
		if (frameTime < frameTimeMin) {
			frameTimeMin = frameTime;
		}
		if (frameTime > frameTimeMax) {
			frameTimeMax = frameTime;
		}
		char frameTimeChar[64];
		snprintf(frameTimeChar, sizeof frameTimeChar, "%.2f", frameTime);

		std::rotate(forwardTimes.begin(), forwardTimes.begin() + 1, forwardTimes.end());
		forwardTimes.back() = (float)forwardTime;
		if (forwardTime < forwardTimeMin) {
			forwardTimeMin = forwardTime;
		}
		if (forwardTime > forwardTimeMax) {
			forwardTimeMax = forwardTime;
		}
		char forwardTimeChar[64];
		snprintf(forwardTimeChar, sizeof forwardTimeChar, "%.2f", forwardTime);

		std::rotate(deferredTimes.begin(), deferredTimes.begin() + 1, deferredTimes.end());
		deferredTimes.back() = (float)deferredTime;
		if (deferredTime < deferredTimeMin) {
			deferredTimeMin = deferredTime;
		}
		if (deferredTime > deferredTimeMax) {
			deferredTimeMax = deferredTime;
		}
		char deferredTimeChar[64];
		snprintf(deferredTimeChar, sizeof deferredTimeChar, "%.2f", deferredTime);

		ImGui::Begin("Menu");
		if (ImGui::CollapsingHeader("Camera"))
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
		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::Text("Direction: ");
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
			ImGui::SameLine(); if (ImGui::InputFloat("x##dir", &(currentSettings.lightDirection.x), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("y##dir", &(currentSettings.lightDirection.y), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("z##dir", &(currentSettings.lightDirection.z), 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) currentSettings.updateSettings = true;
			ImGui::PopItemWidth();
			ImGui::Separator();
			ImGui::Text("Diffuse Colour: ");
			if (ImGui::ColorPicker4("##picker", (float*)&(currentSettings.lightDiffuse), ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview)) currentSettings.updateSettings = true;
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::Text("Current");
			ImGui::ColorButton("##current", ImVec4(currentSettings.lightDiffuse.x, currentSettings.lightDiffuse.y, currentSettings.lightDiffuse.z, currentSettings.lightDiffuse.w), ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(60, 40));
			ImGui::EndGroup();
			ImGui::Separator();
			ImGui::Text("Ambient Colour: ");
			if (ImGui::ColorPicker4("##pickeramb", (float*)&(currentSettings.lightAmbient), ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview)) currentSettings.updateSettings = true;
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::Text("Current");
			ImGui::ColorButton("##currentamb", ImVec4(currentSettings.lightAmbient.x, currentSettings.lightAmbient.y, currentSettings.lightAmbient.z, currentSettings.lightAmbient.w), ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(60, 40));
			ImGui::EndGroup();
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
			if (currentSettings.pipeline == VB_TESSELLATION) if(ImGui::SliderInt("Tess Factor", &(currentSettings.tessellationFactor), 2, 64)) currentSettings.updateSettings = true;
		}
		ImGui::End();

		ImGui::Begin("Statistics");
		if (ImGui::CollapsingHeader("Pass Times"), ImGuiTreeNodeFlags_DefaultOpen)
		{
			ImGui::Text("Frame Times (ms)");
			ImGui::PlotHistogram("", &frameTimes[0], 50, 0, frameTimeChar, frameTimeMin, frameTimeMax, ImVec2(0, 100));
			ImGui::Separator();

			ImGui::Text("Forward Times (ms)");
			ImGui::PlotHistogram("", &forwardTimes[0], 50, 0, forwardTimeChar, forwardTimeMin, forwardTimeMax, ImVec2(0, 100));
			ImGui::Separator();

			ImGui::Text("Deferred Times (ms)");
			ImGui::PlotHistogram("", &deferredTimes[0], 50, 0, deferredTimeChar, deferredTimeMin, deferredTimeMax, ImVec2(0, 100));
			ImGui::Separator();

			if (ImGui::Button("Sample Times", ImVec2(150, 20)))
			{
				SampleFrameTimes();
				SampleForwardTimes();
				SampleDeferredTimes();
			}
			if (frameTimeSample > 0.0f && forwardTimeSample > 0.0f && deferredTimeSample > 0.0f)
			{
				ImGui::Text("Averaged Time Sampled (Last 10 frames)");
				ImGui::Text("Full Frame: %.3f ms", frameTimeSample);
				ImGui::Text("Forward Pass: %.3f ms", forwardTimeSample);
				ImGui::Text("Deferred Pass: %.3f ms", deferredTimeSample);
			}

			ImGui::Separator();

			if (ImGui::Button("Reset Times", ImVec2(150, 20)))
			{
				ResetTimeGraphs();
			}
		}
		if (ImGui::CollapsingHeader("Triangle Counts"), ImGuiTreeNodeFlags_DefaultOpen)
		{
			ImGui::Text("Visibility Buffer Triangle Count: %d", visBuffTriCount);
			ImGui::Text("Tessellated Triangle Count: %d", tessCount);
		}
		ImGui::End();
		ImGui::Render();

		// Only update the application when a value has been changed
		if (currentSettings.updateSettings)
		{
#if IMGUI_ENABLED
			appHandle->ApplySettings(currentSettings);
#endif
			currentSettings.updateSettings = false;
		}
	}
	
	void ImGUI::DrawFrame(VkCommandBuffer commandBuffer)
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}

	void ImGUI::ResetTimeGraphs()
	{
		frameTimes.fill(0.0f);
		frameTimeMin = 9999.0f;
		frameTimeMax = 0.0f;
		forwardTimes.fill(0.0f);
		forwardTimeMin = 9999.0f;
		forwardTimeMax = 0.0f;
		deferredTimes.fill(0.0f);
		deferredTimeMin = 9999.0f;
		deferredTimeMax = 0.0f;
	}

	// Takes last 10 frame times and averages them
	void ImGUI::SampleFrameTimes()
	{
		if (frameTimes[49] != 0.0f)
		{
			float sum = 0.0f;
			for (int i = frameTimes.size() - 1; i > frameTimes.size() - 11; i--)
			{
				sum += frameTimes[i];
			}
			frameTimeSample = sum / 10;
		}
	}
	void ImGUI::SampleForwardTimes()
	{
		if (forwardTimes[49] != 0.0f)
		{
			float sum = 0.0f;
			for (int i = forwardTimes.size() - 1; i > forwardTimes.size() - 11; i--)
			{
				sum += forwardTimes[i];
			}
			forwardTimeSample = sum / 10;
		}
	}
	void ImGUI::SampleDeferredTimes()
	{
		if (deferredTimes[49] != 0.0f)
		{
			float sum = 0.0f;
			for (int i = deferredTimes.size() - 1; i > deferredTimes.size() - 11; i--)
			{
				sum += deferredTimes[i];
			}
			deferredTimeSample = sum / 10;
		}
	}

	void ImGUI::CleanUp()
	{
		vkDestroyDescriptorPool(appHandle->GetVulkanCore()->Device(), descriptorPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}