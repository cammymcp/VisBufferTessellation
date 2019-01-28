#include "VbtImGUI.h"
#include "VulkanApplication.h"
#include "VbtUtils.h"

namespace vbt
{	
	void ImGUI::Init(VulkanApplication* app, GLFWwindow* window, ImGui_ImplVulkan_InitInfo* info, VkRenderPass renderPass, VkCommandPool commandPool)
	{
		// Create vulkan resources
		CreateVulkanResources(app);

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
		vbt::PhysicalDevice physDevice = app->GetVulkanCore()->PhysDevice();
		VkCommandBuffer fontCmd = BeginSingleTimeCommands(info->Device, commandPool);
		ImGui_ImplVulkan_CreateFontsTexture(fontCmd);
		EndSingleTimeCommands(fontCmd, info->Device, physDevice, commandPool);
		ImGui_ImplVulkan_InvalidateFontUploadObjects();
	}

	void ImGUI::CreateVulkanResources(VulkanApplication* app)
	{
		// Descriptor Pool
		std::array<VkDescriptorPoolSize, 1> imGuiPoolSizes = {};
		imGuiPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imGuiPoolSizes[0].descriptorCount = 1;
		VkDescriptorPoolCreateInfo imGuiPoolInfo = {};
		imGuiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		imGuiPoolInfo.poolSizeCount = SCAST_U32(imGuiPoolSizes.size());
		imGuiPoolInfo.pPoolSizes = imGuiPoolSizes.data();
		imGuiPoolInfo.maxSets = SCAST_U32(app->GetVulkanCore()->Swapchain().Images().size());
		if (vkCreateDescriptorPool(app->GetVulkanCore()->Device(), &imGuiPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui descriptor pool");
		}
	}
	
	// Define UI elements to display
	void ImGUI::UpdateFrame(float frameTime)
	{
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

		static float slider = 0.0f;
		ImGui::SetNextWindowSize(ImVec2(375, 200));
		ImGui::Begin("App Settings and Measurements");
		ImGui::PlotHistogram("Frame Times (ms)", &frameTimes[0], 50, 0, frameTimeChar, frameTimeMin, frameTimeMax, ImVec2(0, 100));
		ImGui::End();

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

	void ImGUI::CleanUp()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}