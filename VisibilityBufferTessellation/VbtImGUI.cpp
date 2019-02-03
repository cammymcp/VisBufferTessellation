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
		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_InitInfo initInfo = *info;
		initInfo.DescriptorPool = descriptorPool;
		ImGui_ImplVulkan_Init(&initInfo, renderPass);

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
		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::Text("Position: "); 
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
			ImGui::SameLine(); if (ImGui::InputFloat("x##pos", &(currentSettings.cameraPos.x), 0.0f, 0.0f, "%.1f")) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("y##pos", &(currentSettings.cameraPos.y), 0.0f, 0.0f, "%.1f")) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("z##pos", &(currentSettings.cameraPos.z), 0.0f, 0.0f, "%.1f")) currentSettings.updateSettings = true;
			ImGui::PopItemWidth();
			ImGui::Separator();
			ImGui::Text("Rotation: ");
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
			ImGui::SameLine(); if (ImGui::InputFloat("x##rot", &(currentSettings.cameraRot.x), 0.0f, 0.0f, "%.1f")) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("y##rot", &(currentSettings.cameraRot.y), 0.0f, 0.0f, "%.1f")) currentSettings.updateSettings = true;
			ImGui::SameLine(); if (ImGui::InputFloat("z##rot", &(currentSettings.cameraRot.z), 0.0f, 0.0f, "%.1f")) currentSettings.updateSettings = true;
			ImGui::PopItemWidth();
		}
		ImGui::End();

		// Only update the application when a value has been changed
		if (currentSettings.updateSettings)
		{
			appHandle->UpdateSettings(currentSettings);
			currentSettings.updateSettings = false;
		}

		ImGui::Render();
	}
	
	void ImGUI::DrawFrame(VkCommandBuffer commandBuffer)
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}

	//void ImGUI::AllocateCommandBuffers(VkDevice device, VkCommandPool commandPool, size_t numBuffers)
	//{
	//	commandBuffers.resize(numBuffers);
	//
	//	VkCommandBufferAllocateInfo allocInfo = {};
	//	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	//	allocInfo.commandPool = commandPool;
	//	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	//	allocInfo.commandBufferCount = SCAST_U32(commandBuffers.size());
	//
	//	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to allocate imGUI command buffers");
	//	}
	//
	//	// Create semaphore objects
	//	semaphores.resize(MAX_FRAMES_IN_FLIGHT);
	//	VkSemaphoreCreateInfo semaphoreInfo = {};
	//	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	//	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	//	{
	//		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphores[i]) != VK_SUCCESS)
	//		{
	//			throw std::runtime_error("Failed to create imgui semaphore for a frame");
	//		}
	//	}
	//}
	//
	//void ImGUI::UpdateCommandBuffer(int index)
	//{
	//	VkCommandBufferBeginInfo beginInfo = {};
	//	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	//	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	//	beginInfo.pInheritanceInfo = nullptr; // Optional
	//
	//	if (vkBeginCommandBuffer(commandBuffers[index], &beginInfo) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to begin recording imgui command buffer");
	//	}
	//
	//	// Record Imgui Draw Data and draw funcs into command buffer
	//	DrawFrame(commandBuffers[index]);
	//
	//	// And end recording of command buffers
	//	if (vkEndCommandBuffer(commandBuffers[index]) != VK_SUCCESS)
	//	{
	//		throw std::runtime_error("Failed to update imgui command buffer");
	//	}
	//}

	void ImGUI::ResetFrameGraph()
	{
		frameTimes.fill(0.0f);
		frameTimeMin = 9999.0f;
		frameTimeMax = 0.0f;
	}

	void ImGUI::CleanUp()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}