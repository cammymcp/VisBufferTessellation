#define VMA_IMPLEMENTATION
#include <chrono>
#include "vk_mem_alloc.h"
#include "VulkanApplication.h"
#include "VbtUtils.h"

namespace vbt {

#pragma region Core Functions
void VulkanApplication::InitWindow()
{
	// Init glfw and do not create an OpenGL context
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// Create window
	window = glfwCreateWindow(WIDTH, HEIGHT, title.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, FrameBufferResizeCallback);
	glfwSetKeyCallback(window, ProcessKeyInput);
	glfwSetMouseButtonCallback(window, ProcessMouseInput);
}

void VulkanApplication::Init()
{
	// Initialise core objects and functionality
	vulkan = new VulkanCore();
	vulkan->Init(window);
	InitCamera();
	CreateVmaAllocator();
	InitLight();
	CreateCommandPool();
	CreateTimestampPool();
	CreateRenderPasses();
	CreateShadePassDescriptorSetLayouts();
	CreateVisBuffWritePassDescriptorSetLayout();
	CreateTessWritePassDescriptorSetLayout();
	CreatePipelineCache();
	CreatePipelineLayouts();
	CreateWritePipelines();
	CreateShadePipelines();
	InitialiseTerrains();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateFrameBuffers();
	CreateShadePassDescriptorSets();
	CreateWritePassDescriptorSet();
	CreateTessWritePassDescriptorSet();
#if IMGUI_ENABLED
	InitImGui(currentPipeline == VISIBILITYBUFFER ? visBuffRenderPass : tessRenderPass);
#endif
	AllocateCommandBuffers();
	RecordCommandBuffers();
}

void VulkanApplication::Update()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		UpdateMouse();
#if IMGUI_ENABLED
		imGui.Update(frameTime, forwardPassTime, deferredPassTime, camera.Position(), camera.Rotation(), light.Direction(), light.Diffuse(), light.Ambient());
#endif
		
		// Draw frame and calculate frame time
		auto frameStart = std::chrono::high_resolution_clock::now();
		DrawFrame();
		auto frameEnd = std::chrono::high_resolution_clock::now();
		auto diff = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
		frameTime = diff / 1000.0;

		GetTimestampResults();

		camera.Update(frameTime);
	}

	// Wait for the device to finish up any operations when exiting the main loop
	vkDeviceWaitIdle(vulkan->Device());
}

void VulkanApplication::CleanUp()
{
	CleanUpSwapChainResources(); 

	// Destroy Descriptor Pool
	vkDestroyDescriptorPool(vulkan->Device(), descriptorPool, nullptr);

	// Destroy query pool
	vkDestroyQueryPool(vulkan->Device(), timestampPool, nullptr);

	// Destroy descriptor layouts
	vkDestroyDescriptorSetLayout(vulkan->Device(), visBuffShadePassDescSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkan->Device(), visBuffWritePassDescSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkan->Device(), tessWritePassDescSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkan->Device(), tessShadePassDescSetLayout, nullptr);

	// Destroy uniform buffers
	light.CleanUp(allocator);
	mvpUniformBuffer.CleanUp(allocator);
	settingsBuffer.CleanUp(allocator);

	// Destroy vertex and index buffers
	visBuffTerrain.CleanUp(allocator, vulkan->Device());
	tessTerrain.CleanUp(allocator, vulkan->Device());

#if IMGUI_ENABLED
	// Destroy ImGui resources
	imGui.CleanUp();
#endif

	vmaDestroyAllocator(allocator);

	// Destroy command pool
	vkDestroyCommandPool(vulkan->Device(), commandPool, nullptr);

	// Clean up Vulkan core objects
	vulkan->CleanUp();
	delete vulkan;
	vulkan = NULL;

	// Destroy window and terminate glfw
	glfwDestroyWindow(window);
	glfwTerminate();
}
#pragma endregion

#if IMGUI_ENABLED
#pragma region ImGui Functions
void VulkanApplication::InitImGui(VkRenderPass renderPass)
{
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = vulkan->Instance();
	initInfo.PhysicalDevice = vulkan->PhysDevice().VkHandle();
	initInfo.Device = vulkan->Device();
	initInfo.QueueFamily = PhysicalDevice::FindQueueFamilies(vulkan->PhysDevice().VkHandle(), vulkan->Swapchain().Surface()).graphicsFamily.value();
	initInfo.Queue = vulkan->PhysDevice().Queues()->graphics;
	initInfo.PipelineCache = pipelineCache;
	initInfo.Allocator = nullptr;
	initInfo.CheckVkResultFn = ImGuiCheckVKResult;
	imGui.Init(this, window, &initInfo, renderPass, commandPool, visBuffTerrainTriCount, tessTerrainTriCount);
	imGui.Update(0.0, 0.0, 0.0, camera.Position(), camera.Rotation(), light.Direction(), light.Diffuse(), light.Ambient()); // Update imgui frame once to populate buffers
}

void VulkanApplication::RecreateImGui(VkRenderPass renderPass)
{
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = vulkan->Instance();
	initInfo.PhysicalDevice = vulkan->PhysDevice().VkHandle();
	initInfo.Device = vulkan->Device();
	initInfo.QueueFamily = PhysicalDevice::FindQueueFamilies(vulkan->PhysDevice().VkHandle(), vulkan->Swapchain().Surface()).graphicsFamily.value();
	initInfo.Queue = vulkan->PhysDevice().Queues()->graphics;
	initInfo.PipelineCache = pipelineCache;
	initInfo.Allocator = nullptr;
	initInfo.CheckVkResultFn = ImGuiCheckVKResult;
	imGui.Recreate(&initInfo, renderPass, commandPool);
	imGui.Update(0.0f, 0.0, 0.0, camera.Position(), camera.Rotation(), light.Direction(), light.Diffuse(), light.Ambient());
}

void VulkanApplication::ApplySettings(AppSettings settings)
{
	// Camera
	camera.SetPosition(settings.cameraPos);
	camera.SetRotation(settings.cameraRot);

	// Light
	light.SetDiffuse(settings.lightDiffuse);
	light.SetDirection(glm::vec4(settings.lightDirection, 1.0f));
	light.SetAmbient(settings.lightAmbient);

	// Render settings
	renderSettingsUbo.tessellationFactor = settings.tessellationFactor;
	renderSettingsUbo.showTessCoordsBuffer = settings.showTessBuff;
	renderSettingsUbo.showVisibilityBuffer = settings.showVisBuff;
	renderSettingsUbo.showInterpolatedTex = settings.showInterpTex;
	renderSettingsUbo.wireframe = settings.wireframe;

	// Check for pipeline change
	if (settings.pipeline != currentPipeline)
	{
		// Command buffers are re-recorded every frame to satisfy ImGui, so changing the local current pipeline will automatically bind the new pipeline and renderpass objects for the next frame. 
		currentPipeline = settings.pipeline;

		// Wait for current operations to be finished
		vkDeviceWaitIdle(vulkan->Device());

		// Need to reinitialise ImGui to be compatible with new renderpass
		RecreateImGui(currentPipeline == VISIBILITYBUFFER ? visBuffRenderPass : tessRenderPass);
	}
}
#pragma endregion
#endif

#pragma region Geometry Functions
void VulkanApplication::InitialiseTerrains()
{
	Terrain::InitInfo visBuffTerrainInfo;
	visBuffTerrainInfo.subdivisions = 542; 
	visBuffTerrainInfo.width = 64;
	visBuffTerrainInfo.uvScale = 10.0f;
	Terrain::InitInfo tessTerrainInfo;
	tessTerrainInfo.subdivisions = 14;
	tessTerrainInfo.width = 64;
	tessTerrainInfo.uvScale = 10.75f;

	// Generate terrain geometry
	visBuffTerrainTriCount = visBuffTerrain.Init(allocator, vulkan->Device(), vulkan->PhysDevice(), commandPool, visBuffTerrainInfo);
	tessTerrainTriCount = tessTerrain.Init(allocator, vulkan->Device(), vulkan->PhysDevice(), commandPool, tessTerrainInfo);
}
#pragma endregion

#pragma region Testing Functions
void VulkanApplication::CreateTimestampPool()
{
	VkQueryPoolCreateInfo timestampPoolInfo = {};
	timestampPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	timestampPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	timestampPoolInfo.queryCount = 4; // Start of forward, end of forward, start of deferred, end of deferred. 
	timestampPoolInfo.pNext = NULL;
	timestampPoolInfo.flags = 0;

	if (vkCreateQueryPool(vulkan->Device(), &timestampPoolInfo, nullptr, &timestampPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Query pool creation failed");
	}
}

void VulkanApplication::GetTimestampResults()
{
	// Requests the results of the timestamp queries made in the current frame
	std::array<uint64_t, 4> timestamps;

	if (vkGetQueryPoolResults(vulkan->Device(), timestampPool, 0, SCAST_U32(timestamps.size()), SCAST_U32(timestamps.size()) * sizeof(uint64_t), timestamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to get timestamp results");
	}
	else
	{
		forwardPassTime = ((double)timestamps[1] - (double)timestamps[0]) / 1000000.0;
		deferredPassTime = ((double)timestamps[3] - (double)timestamps[2]) / 1000000.0;
	}
}
#pragma endregion

#pragma region Input Functions
// Callback used by glfw when keyboard or mouse input is recieved
void VulkanApplication::ProcessKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Get vulkan app instance from window user pointer
	void *data = glfwGetWindowUserPointer(window);
	VulkanApplication* vulkanApp = static_cast<VulkanApplication*>(data);

	// Camera movement
	if (key == GLFW_KEY_W)
	{
		if (action == GLFW_PRESS)
			vulkanApp->camera.input.forward = true;
		else if (action == GLFW_RELEASE)
			vulkanApp->camera.input.forward = false;
	}
	if (key == GLFW_KEY_S)
	{
		if (action == GLFW_PRESS)
			vulkanApp->camera.input.back = true;
		else if (action == GLFW_RELEASE)
			vulkanApp->camera.input.back = false;
	}
	if (key == GLFW_KEY_A)
	{
		if (action == GLFW_PRESS)
			vulkanApp->camera.input.left = true;
		else if (action == GLFW_RELEASE)
			vulkanApp->camera.input.left = false;
	}
	if (key == GLFW_KEY_D)
	{
		if (action == GLFW_PRESS)
			vulkanApp->camera.input.right = true;
		else if (action == GLFW_RELEASE)
			vulkanApp->camera.input.right = false;
	}
	if (key == GLFW_KEY_Q)
	{
		if (action == GLFW_PRESS)
			vulkanApp->camera.input.up = true;
		else if (action == GLFW_RELEASE)
			vulkanApp->camera.input.up = false;
	}
	if (key == GLFW_KEY_E)
	{
		if (action == GLFW_PRESS)
			vulkanApp->camera.input.down = true;
		else if (action == GLFW_RELEASE)
			vulkanApp->camera.input.down = false;
	}
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
		vulkanApp->camera.Reset();
	if (key == GLFW_KEY_T && action == GLFW_PRESS)
	{
		// Switch to tessellation pipeline
		vulkanApp->SwitchPipeline(VB_TESSELLATION);
	}
	if (key == GLFW_KEY_V && action == GLFW_PRESS)
	{
		// Switch to vis buff pipeline
		vulkanApp->SwitchPipeline(VISIBILITYBUFFER);
	}
}

void VulkanApplication::ProcessMouseInput(GLFWwindow* window, int button, int action, int mods)
{
	// Get vulkan app instance from window user pointer
	void *data = glfwGetWindowUserPointer(window);
	VulkanApplication* vulkanApp = static_cast<VulkanApplication*>(data);	

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		vulkanApp->mouseLeftDown = true;
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
		vulkanApp->mouseLeftDown = false;	
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		vulkanApp->mouseRightDown = true;
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
		vulkanApp->mouseRightDown = false;
}

// Rotates camera with mouse movement
void VulkanApplication::UpdateMouse()
{
	double xPos, yPos;
	glfwGetCursorPos(window, &xPos, &yPos);
	int deltaX = (int)mousePosition.x - (int)xPos;
	int deltaY = (int)mousePosition.y - (int)yPos;
	mousePosition = { xPos, yPos };
	
	if (mouseRightDown)
	{
		camera.Rotate(glm::vec3(-deltaY * camera.rotateSpeed, -deltaX * camera.rotateSpeed, 0.0f));
	}
}

void VulkanApplication::SwitchPipeline(PipelineType type)
{
	if (currentPipeline != type)
	{
		// Command buffers are re-recorded every frame, so changing the local current pipeline will automatically bind the new pipeline and renderpass objects for the next frame. 
		currentPipeline = type;

		// Wait for current operations to be finished
		vkDeviceWaitIdle(vulkan->Device());

#if IMGUI_ENABLED
		// Need to reinitialise ImGui to be compatible with new renderpass
		RecreateImGui(currentPipeline == VISIBILITYBUFFER ? visBuffRenderPass : tessRenderPass);
#endif
	}
}
#pragma endregion

#pragma region Presentation and Swap Chain Functions
// Called when Vulkan tells us that the swap chain is no longer optimal
void VulkanApplication::RecreateSwapChain()
{
	// Pause execution if minimised
	int width = 0, height = 0;
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	// Wait for current operations to be finished
	vkDeviceWaitIdle(vulkan->Device());

	vulkan->Swapchain().CleanUpSwapChain(vulkan->Device());
	CleanUpSwapChainResources();

	// Recreate required objects
	vulkan->RecreateSwapchain(window);
	CreateRenderPasses();
	CreatePipelineCache();
	CreatePipelineLayouts();
	CreateWritePipelines();
	CreateShadePipelines();
	CreateFrameBuffers();
	RecordCommandBuffers();
}

void VulkanApplication::CleanUpSwapChainResources()
{
	// Destroy frame buffers
	for (size_t i = 0; i < visBuffFramebuffers.size(); i++)
		vkDestroyFramebuffer(vulkan->Device(), visBuffFramebuffers[i], nullptr);
	for (size_t i = 0; i < tessFramebuffers.size(); i++)
		vkDestroyFramebuffer(vulkan->Device(), tessFramebuffers[i], nullptr);

	// Destroy visibility buffer images
	visibilityBuffer.visibility.CleanUp(allocator, vulkan->Device());
	tessVisibilityBuffer.visibility.CleanUp(allocator, vulkan->Device());
	tessVisibilityBuffer.tessCoords_v1XYZ_v2X.CleanUp(allocator, vulkan->Device());
	tessVisibilityBuffer.tessCoords_v2YZ_v3XY.CleanUp(allocator, vulkan->Device());
	tessVisibilityBuffer.tessCoords_v3Z.CleanUp(allocator, vulkan->Device());
	depthImage.CleanUp(allocator, vulkan->Device());

	// Free command buffers
	vkFreeCommandBuffers(vulkan->Device(), commandPool, SCAST_U32(commandBuffers.size()), commandBuffers.data());
	vkDestroyPipeline(vulkan->Device(), visBuffShadePipeline, nullptr);
	vkDestroyPipeline(vulkan->Device(), visBuffWritePipeline, nullptr);
	vkDestroyPipeline(vulkan->Device(), tessShadePipeline, nullptr);
	vkDestroyPipeline(vulkan->Device(), tessWritePipeline, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), visBuffShadePipelineLayout, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), visBuffWritePipelineLayout, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), tessShadePipelineLayout, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), tessWritePipelineLayout, nullptr);
	vkDestroyPipelineCache(vulkan->Device(), pipelineCache, nullptr);
	vkDestroyRenderPass(vulkan->Device(), visBuffRenderPass, nullptr);
	vkDestroyRenderPass(vulkan->Device(), tessRenderPass, nullptr);
}
#pragma endregion

#pragma region Graphics Pipeline Functions
void VulkanApplication::CreatePipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (vkCreatePipelineCache(vulkan->Device(), &pipelineCacheCreateInfo, nullptr, &pipelineCache) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline cache");
	}
}

// Creation of the graphics pipeline requires four objects:
// Shader stages: the shader modules that define the functionality of the programmable stages of the pipeline
// Fixed-function state: all of the structures that define the fixed-function stages of the pipeline
// Pipeline Layout: the uniform and push values referenced by the shader that can be updated at draw time
// Render pass: the attachments referenced by the pipeline stages and their usage
void VulkanApplication::CreateShadePipelines()
{
	// Create vis buff shade shader stages from compiled shader code
	auto vertShaderCode = ReadFile("shaders/visbuffshade.vert.spv");
	auto fragShaderCode = ReadFile("shaders/visbuffshade.frag.spv");

	// Create shader modules
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	vertShaderModule = CreateShaderModule(vertShaderCode);
	fragShaderModule = CreateShaderModule(fragShaderCode);

	// Create shader stages
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {}; // Vertex
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {}; // Fragment
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };	

	// Set up topology input format
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Now create the viewport state with viewport and scissor
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = nullptr;
	viewportState.scissorCount = 1;
	viewportState.pScissors = nullptr;

	// Set up the rasterizer, wireframe can be set here
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE; // Fragments beyond near and far planes are clamped instead of discarded. Useful for shadow mapping but requires GPU feature
	rasterizer.rasterizerDiscardEnable = VK_FALSE; // Geometry never passes through rasterizer if this is true.
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Cull back faces
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Faces are drawn counter clockwise to be considered front-facing
	rasterizer.depthBiasEnable = VK_FALSE;

	// Set up depth test
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// Set up multisampling (disabled)
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Set up color blending (disabled, all fragment colors will go to the framebuffer unmodified)
	VkPipelineColorBlendAttachmentState colourBlendAttachment = {};
	colourBlendAttachment.colorWriteMask = 0xf;
	colourBlendAttachment.blendEnable = VK_FALSE;
	std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachments = { colourBlendAttachment };
	VkPipelineColorBlendStateCreateInfo colourBlending = {};
	colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlending.logicOpEnable = VK_FALSE;
	colourBlending.attachmentCount = SCAST_U32(blendAttachments.size());
	colourBlending.pAttachments = blendAttachments.data();

	// Dynamic State
	std::vector<VkDynamicState> dynamicStateEnables = {	VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR	};
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = SCAST_U32(dynamicStateEnables.size());
	dynamicState.flags = 0;

	// We now have everything we need to create the vis buff shade graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colourBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = visBuffShadePipelineLayout;
	pipelineInfo.renderPass = visBuffRenderPass;
	pipelineInfo.subpass = 1; // Index of the sub pass where this pipeline will be used
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; 
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.flags = 0;
	// Empty vertex input state, fullscreen triangle is generated by the vertex shader
	VkPipelineVertexInputStateCreateInfo emptyInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	pipelineInfo.pVertexInputState = &emptyInputState;
	if (vkCreateGraphicsPipelines(vulkan->Device(), pipelineCache, 1, &pipelineInfo, nullptr, &visBuffShadePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff shade pipeline");
	}

	// Clean up shader module objects
	vkDestroyShaderModule(vulkan->Device(), vertShaderModule, nullptr);
	vkDestroyShaderModule(vulkan->Device(), fragShaderModule, nullptr);

	// Tessellation shade pipeline
	// Create shader stages
	vertShaderCode = ReadFile("shaders/tessshade.vert.spv");
	fragShaderCode = ReadFile("shaders/tessshade.frag.spv");
	VkShaderModule tessVertShaderModule;
	VkShaderModule tessFragShaderModule;
	tessVertShaderModule = CreateShaderModule(vertShaderCode);
	tessFragShaderModule = CreateShaderModule(fragShaderCode);
	vertShaderStageInfo.module = tessVertShaderModule;
	fragShaderStageInfo.module = tessFragShaderModule;
	shaderStages[0] = vertShaderStageInfo;
	shaderStages[1] = fragShaderStageInfo;

	// We can now reuse most of the data from the vis buff shade pipeline setup
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.layout = tessShadePipelineLayout;
	pipelineInfo.renderPass = tessRenderPass;
	if (vkCreateGraphicsPipelines(vulkan->Device(), pipelineCache, 1, &pipelineInfo, nullptr, &tessShadePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create tess shade pipeline");
	}

	// Clean up shader module objects
	vkDestroyShaderModule(vulkan->Device(), tessVertShaderModule, nullptr);
	vkDestroyShaderModule(vulkan->Device(), tessFragShaderModule, nullptr);
}

void VulkanApplication::CreateWritePipelines()
{
	// Create visibility buffer write shader stages from compiled shader code
	auto vertShaderCode = ReadFile("shaders/visbuffwrite.vert.spv");
	auto fragShaderCode = ReadFile("shaders/visbuffwrite.frag.spv");

	// Create shader modules
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	vertShaderModule = CreateShaderModule(vertShaderCode);
	fragShaderModule = CreateShaderModule(fragShaderCode);

	// Create shader stages
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {}; // Vertex
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {}; // Fragment
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo visBuffWriteShaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Set up vertex input format for geometry pass
	auto bindingDescription = Vertex::GetBindingDescription();
	auto attributeDescriptions = Vertex::GetAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = SCAST_U32(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// Set up topology input format
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Now create the viewport state with viewport and scissor
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = nullptr;
	viewportState.scissorCount = 1;
	viewportState.pScissors = nullptr;

	// Set up the rasterizer, wireframe can be set here
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE; // Fragments beyond near and far planes are clamped instead of discarded. Useful for shadow mapping but requires GPU feature
	rasterizer.rasterizerDiscardEnable = VK_FALSE; // Geometry never passes through rasterizer if this is true.
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Cull back faces
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // Faces are drawn counter clockwise to be considered front-facing
	rasterizer.depthBiasEnable = VK_FALSE;

	// Set up depth test
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// Set up multisampling (disabled)
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// We need to set up color blend attachments for all of the visibility buffer color attachments in the subpass
	VkPipelineColorBlendAttachmentState emptyBlendAttachment = {};
	emptyBlendAttachment.colorWriteMask = 0xf;
	emptyBlendAttachment.blendEnable = VK_FALSE;
	std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments = { emptyBlendAttachment, emptyBlendAttachment };
	VkPipelineColorBlendStateCreateInfo colourBlending = {};
	colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlending.logicOpEnable = VK_FALSE;
	colourBlending.attachmentCount = SCAST_U32(blendAttachments.size());
	colourBlending.pAttachments = blendAttachments.data();

	// Dynamic State
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = SCAST_U32(dynamicStateEnables.size());
	dynamicState.flags = 0;

	// We now have everything we need to create the vis buff write graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.layout = visBuffWritePipelineLayout;
	pipelineInfo.renderPass = visBuffRenderPass;
	pipelineInfo.subpass = 0; // Index of the sub pass where this pipeline will be used
	pipelineInfo.pStages = visBuffWriteShaderStages;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colourBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	// Now create the vis buff write pass pipeline
	if (vkCreateGraphicsPipelines(vulkan->Device(), pipelineCache, 1, &pipelineInfo, nullptr, &visBuffWritePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff write pipeline");
	}

	// Clean up shader module objects
	vkDestroyShaderModule(vulkan->Device(), vertShaderModule, nullptr);
	vkDestroyShaderModule(vulkan->Device(), fragShaderModule, nullptr);

	// Create visibility buffer tessellation write shader stages
	vertShaderCode = ReadFile("shaders/tesswrite.vert.spv");
	auto hullShaderCode = ReadFile("shaders/tesswrite.tesc.spv");
	auto domainShaderCode = ReadFile("shaders/tesswrite.tese.spv");
	auto geomShaderCode = ReadFile("shaders/tesswrite.geom.spv");
	fragShaderCode = ReadFile("shaders/tesswrite.frag.spv");

	// Create shader modules
	VkShaderModule tessVertShaderModule;
	VkShaderModule hullShaderModule;
	VkShaderModule domainShaderModule;
	VkShaderModule geometryShaderModule;
	VkShaderModule tessFragShaderModule;
	tessVertShaderModule = CreateShaderModule(vertShaderCode);
	hullShaderModule = CreateShaderModule(hullShaderCode);
	domainShaderModule = CreateShaderModule(domainShaderCode);
	geometryShaderModule = CreateShaderModule(geomShaderCode);
	tessFragShaderModule = CreateShaderModule(fragShaderCode);

	// Create shader stages
	vertShaderStageInfo.module = tessVertShaderModule; // Vert
	VkPipelineShaderStageCreateInfo hullShaderStageInfo = {}; // Hull
	hullShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	hullShaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	hullShaderStageInfo.module = hullShaderModule;
	hullShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo domainShaderStageInfo = {}; // Domain
	domainShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	domainShaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	domainShaderStageInfo.module = domainShaderModule;
	domainShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo geometryShaderStageInfo = {}; // Geometry
	geometryShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	geometryShaderStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	geometryShaderStageInfo.module = geometryShaderModule;
	geometryShaderStageInfo.pName = "main";
	fragShaderStageInfo.module = tessFragShaderModule; // Frag
	VkPipelineShaderStageCreateInfo tessWriteShaderStages[] = { vertShaderStageInfo, hullShaderStageInfo, domainShaderStageInfo, geometryShaderStageInfo, fragShaderStageInfo };

	// Set up topology input format
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

	// Set up tessellation state
	VkPipelineTessellationStateCreateInfo tessStateInfo = {};
	tessStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tessStateInfo.patchControlPoints = 3;

	// We need to set up color blend attachments for all of the visibility buffer color attachments in the subpass
	std::array<VkPipelineColorBlendAttachmentState, 5> tessBlendAttachments = { emptyBlendAttachment, emptyBlendAttachment, emptyBlendAttachment, emptyBlendAttachment, emptyBlendAttachment };
	colourBlending.attachmentCount = SCAST_U32(tessBlendAttachments.size());
	colourBlending.pAttachments = tessBlendAttachments.data();

	// We now have everything we need to create the tess write graphics pipeline
	pipelineInfo.layout = tessWritePipelineLayout;
	pipelineInfo.renderPass = tessRenderPass;
	pipelineInfo.pStages = tessWriteShaderStages;
	pipelineInfo.stageCount = 5;
	pipelineInfo.pTessellationState = &tessStateInfo;
	if (vkCreateGraphicsPipelines(vulkan->Device(), pipelineCache, 1, &pipelineInfo, nullptr, &tessWritePipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create tess write pipeline");
	}

	// Clean up shader module objects
	vkDestroyShaderModule(vulkan->Device(), tessVertShaderModule, nullptr);
	vkDestroyShaderModule(vulkan->Device(), hullShaderModule, nullptr);
	vkDestroyShaderModule(vulkan->Device(), domainShaderModule, nullptr);
	vkDestroyShaderModule(vulkan->Device(), geometryShaderModule, nullptr);
	vkDestroyShaderModule(vulkan->Device(), tessFragShaderModule, nullptr);
}

void VulkanApplication::CreatePipelineLayouts()
{
	// Vis Buff write layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &visBuffWritePassDescSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &visBuffWritePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create geometry pipeline layout");
	}

	// Vis Buff Shade Layout
	pipelineLayoutInfo.pSetLayouts = &visBuffShadePassDescSetLayout;
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &visBuffShadePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff shade pipeline layout");
	}

	// Tess write layout
	pipelineLayoutInfo.pSetLayouts = &tessWritePassDescSetLayout;
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &tessWritePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff tess write pipeline layout");
	}

	// Tess Shade Layout
	pipelineLayoutInfo.pSetLayouts = &tessShadePassDescSetLayout;
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &tessShadePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff tess shade pipeline layout");
	}
}

void VulkanApplication::CreateRenderPasses()
{
	// Setup images for use as frame buffer attachments
	CreateFrameBufferAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &visibilityBuffer.visibility, allocator); // 32 bit uint will be unpacked into four 8bit floats
	CreateFrameBufferAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &tessVisibilityBuffer.visibility, allocator); 
	CreateFrameBufferAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &tessVisibilityBuffer.tessCoords_v1XYZ_v2X, allocator);
	CreateFrameBufferAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &tessVisibilityBuffer.tessCoords_v2YZ_v3XY, allocator);
	CreateFrameBufferAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &tessVisibilityBuffer.tessCoords_v3Z, allocator);
	CreateDepthResources();

	// Create attachment descriptions
	// Swapchain image attachment
	VkAttachmentDescription swapChainAttachmentDesc = {};
	swapChainAttachmentDesc.format = vulkan->Swapchain().ImageFormat();
	swapChainAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	swapChainAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapChainAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapChainAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapChainAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapChainAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapChainAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Visibility Attachment
	VkAttachmentDescription visibilityAttachmentDesc = {};
	visibilityAttachmentDesc.format = visibilityBuffer.visibility.Format();
	visibilityAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	visibilityAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	visibilityAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	visibilityAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	visibilityAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	visibilityAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	visibilityAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	// Tess Visibility attachment
	VkAttachmentDescription tessVisibilityAttachmentDesc = visibilityAttachmentDesc;
	tessVisibilityAttachmentDesc.format = tessVisibilityBuffer.visibility.Format();
	// Tess Coords attachments
	VkAttachmentDescription tessCoordsAttachmentDesc1 = tessVisibilityAttachmentDesc;
	tessCoordsAttachmentDesc1.format = tessVisibilityBuffer.tessCoords_v1XYZ_v2X.Format();
	VkAttachmentDescription tessCoordsAttachmentDesc2 = tessVisibilityAttachmentDesc;
	tessCoordsAttachmentDesc2.format = tessVisibilityBuffer.tessCoords_v2YZ_v3XY.Format();
	VkAttachmentDescription tessCoordsAttachmentDesc3 = tessVisibilityAttachmentDesc;
	tessCoordsAttachmentDesc3.format = tessVisibilityBuffer.tessCoords_v3Z.Format();
	// Depth attachment
	VkAttachmentDescription depthAttachmentDesc = {};
	depthAttachmentDesc.format = depthImage.Format();
	depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Subpass dependencies will be the same for both renderpasses
	std::array<VkSubpassDependency, 3> dependencies = {};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Transition the vis buffer from color attachment to shader read
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 0;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Visibility Buffer RenderPass =============================================
	std::array<VkAttachmentDescription, 3> visBuffAttachments = {};
	visBuffAttachments[0] = swapChainAttachmentDesc;
	visBuffAttachments[1] = visibilityAttachmentDesc;
	visBuffAttachments[2] = depthAttachmentDesc;

	// Two subpasses
	std::array<VkSubpassDescription, 2> visBuffSubpassDescriptions{};

	// First Subpass: Visibility Buffer Write
	// --------------------------------------
	// Attachment references 
	std::vector<VkAttachmentReference> visBuffWriteColorReferences;
	visBuffWriteColorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	visBuffWriteColorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }); // Visibility Buffer attachment
	VkAttachmentReference depthReference = { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	// Subpass Description
	visBuffSubpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Specify that this is a graphics subpass, not compute
	visBuffSubpassDescriptions[0].colorAttachmentCount = SCAST_U32(visBuffWriteColorReferences.size());;
	visBuffSubpassDescriptions[0].pColorAttachments = visBuffWriteColorReferences.data(); // Important: The attachment's index in this array is what is referenced in the out variable of the shader!
	visBuffSubpassDescriptions[0].pDepthStencilAttachment = &depthReference;

	// Second Subpass: Visibility Buffer Shade
	// --------------------------------------
	// Attachment references 
	std::vector<VkAttachmentReference> visBuffShadeColourReferences;
	visBuffShadeColourReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	std::vector <VkAttachmentReference> inputReferences;
	inputReferences.push_back({ 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }); // Visibility Buffer attachment

	// Subpass Description
	visBuffSubpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	visBuffSubpassDescriptions[1].colorAttachmentCount = SCAST_U32(visBuffShadeColourReferences.size());
	visBuffSubpassDescriptions[1].pColorAttachments = visBuffShadeColourReferences.data();
	visBuffSubpassDescriptions[1].pDepthStencilAttachment = &depthReference;
	// Input attachments from vis buff write subpass
	visBuffSubpassDescriptions[1].inputAttachmentCount = SCAST_U32(inputReferences.size());
	visBuffSubpassDescriptions[1].pInputAttachments = inputReferences.data();
	// --------------------------------------

	// Create the render pass with required attachments
	VkRenderPassCreateInfo visBuffRenderPassInfo = {};
	visBuffRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	visBuffRenderPassInfo.attachmentCount = SCAST_U32(visBuffAttachments.size());
	visBuffRenderPassInfo.pAttachments = visBuffAttachments.data();
	visBuffRenderPassInfo.subpassCount = SCAST_U32(visBuffSubpassDescriptions.size());
	visBuffRenderPassInfo.pSubpasses = visBuffSubpassDescriptions.data();
	visBuffRenderPassInfo.dependencyCount = SCAST_U32(dependencies.size());
	visBuffRenderPassInfo.pDependencies = dependencies.data();

	// Create visibility buffer renderpass
	if (vkCreateRenderPass(vulkan->Device(), &visBuffRenderPassInfo, nullptr, &visBuffRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass");
	}
	// ==========================================================================

	// Tessellataion RenderPass =================================================
	std::array<VkAttachmentDescription, 6> tessAttachments = {};
	tessAttachments[0] = swapChainAttachmentDesc;
	tessAttachments[1] = tessVisibilityAttachmentDesc;
	tessAttachments[2] = tessCoordsAttachmentDesc1;
	tessAttachments[3] = tessCoordsAttachmentDesc2;
	tessAttachments[4] = tessCoordsAttachmentDesc3;
	tessAttachments[5] = depthAttachmentDesc;

	// Two subpasses
	std::array<VkSubpassDescription, 2> tessSubpassDescriptions{};

	// First Subpass: Tessellation Write pass
	// --------------------------------------
	// Attachment references 
	std::vector<VkAttachmentReference> tessWriteColorReferences;
	tessWriteColorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }); // Swapchain image
	tessWriteColorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }); // Visibility Buffer attachment
	tessWriteColorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }); // Tess Coords 1 attachment
	tessWriteColorReferences.push_back({ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }); // Tess Coords 2 attachment
	tessWriteColorReferences.push_back({ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }); // Tess Coords 3 attachment
	VkAttachmentReference tessDepthReference = { 5, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	// Subpass Description
	tessSubpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Specify that this is a graphics subpass, not compute
	tessSubpassDescriptions[0].colorAttachmentCount = SCAST_U32(tessWriteColorReferences.size());;
	tessSubpassDescriptions[0].pColorAttachments = tessWriteColorReferences.data(); 
	tessSubpassDescriptions[0].pDepthStencilAttachment = &tessDepthReference;

	// Second Subpass: Tessellation Shade Pass
	// --------------------------------------
	// Attachment references 
	std::vector<VkAttachmentReference> tessShadeColourReferences;
	tessShadeColourReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	std::vector<VkAttachmentReference> tessInputReferences;
	tessInputReferences.push_back({ 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }); // Visibility Buffer attachment
	tessInputReferences.push_back({ 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }); // Tess Coords 1 attachment
	tessInputReferences.push_back({ 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }); // Tess Coords 2 attachment
	tessInputReferences.push_back({ 4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }); // Tess Coords 3 attachment

	// Subpass Description
	tessSubpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	tessSubpassDescriptions[1].colorAttachmentCount = SCAST_U32(tessShadeColourReferences.size());
	tessSubpassDescriptions[1].pColorAttachments = tessShadeColourReferences.data();
	tessSubpassDescriptions[1].pDepthStencilAttachment = &tessDepthReference;
	// Input attachments from vis buff write subpass
	tessSubpassDescriptions[1].inputAttachmentCount = SCAST_U32(tessInputReferences.size());
	tessSubpassDescriptions[1].pInputAttachments = tessInputReferences.data();
	// --------------------------------------

	// Create the render pass with required attachments
	VkRenderPassCreateInfo tessRenderPassInfo = {};
	tessRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	tessRenderPassInfo.attachmentCount = SCAST_U32(tessAttachments.size());
	tessRenderPassInfo.pAttachments = tessAttachments.data();
	tessRenderPassInfo.subpassCount = SCAST_U32(tessSubpassDescriptions.size());
	tessRenderPassInfo.pSubpasses = tessSubpassDescriptions.data();
	tessRenderPassInfo.dependencyCount = SCAST_U32(dependencies.size());
	tessRenderPassInfo.pDependencies = dependencies.data();

	// Create tessellation renderpass
	if (vkCreateRenderPass(vulkan->Device(), &tessRenderPassInfo, nullptr, &tessRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create tessellation render pass");
	}
	// ==========================================================================
}

VkShaderModule VulkanApplication::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(vulkan->Device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module");
	}

	return shaderModule;
}
#pragma endregion

#pragma region Drawing Functions
void VulkanApplication::InitCamera()
{
	camera.SetPerspective(45.0f, (float)vulkan->Swapchain().Extent().width / (float)vulkan->Swapchain().Extent().height, 0.1f, 500.0f, true);
	camera.SetRotation(glm::vec3(10.0f, 310.0f, 0.0f), true);
	camera.SetPosition(glm::vec3(-2.0f, -6.0f, -1.5f), true);
}

void VulkanApplication::InitLight()
{
	DirectionalLight::InitInfo lightInfo = {};
	lightInfo.direction = glm::vec4(-0.8944f, -0.4472f, 0.0f, 1.0f);
	lightInfo.diffuse = glm::vec4(0.818f, 0.713f, 0.556f, 1.0f);
	lightInfo.ambient = glm::vec4(0.4f, 0.3f, 0.3f, 1.0f);

	light.Init(lightInfo, allocator);
}

void VulkanApplication::CreateFrameBuffers()
{
	// Create Visibility Buffer frame buffers
	std::array<VkImageView, 3> visBuffAttachments = {};

	VkFramebufferCreateInfo visBuffFramebufferInfo = {};
	visBuffFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	visBuffFramebufferInfo.pNext = NULL;
	visBuffFramebufferInfo.renderPass = visBuffRenderPass; // Tell frame buffer which render pass it should be compatible with
	visBuffFramebufferInfo.attachmentCount = SCAST_U32(visBuffAttachments.size());
	visBuffFramebufferInfo.pAttachments = visBuffAttachments.data();
	visBuffFramebufferInfo.width = vulkan->Swapchain().Extent().width;
	visBuffFramebufferInfo.height = vulkan->Swapchain().Extent().height;
	visBuffFramebufferInfo.layers = 1;

	visBuffFramebuffers.resize(vulkan->Swapchain().ImageViews().size());
	for (size_t i = 0; i < vulkan->Swapchain().ImageViews().size(); i++)
	{
		visBuffAttachments[0] = vulkan->Swapchain().ImageViews()[i];
		visBuffAttachments[1] = visibilityBuffer.visibility.ImageView();
		visBuffAttachments[2] = depthImage.ImageView();

		if (vkCreateFramebuffer(vulkan->Device(), &visBuffFramebufferInfo, nullptr, &visBuffFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create vis buff frame buffer");
		}
	}

	// Tessellation Pipeline
	std::array<VkImageView, 6> tessAttachments = {};

	VkFramebufferCreateInfo tessFramebufferInfo = {};
	tessFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	tessFramebufferInfo.pNext = NULL;
	tessFramebufferInfo.renderPass = tessRenderPass; 
	tessFramebufferInfo.attachmentCount = SCAST_U32(tessAttachments.size());
	tessFramebufferInfo.pAttachments = tessAttachments.data();
	tessFramebufferInfo.width = vulkan->Swapchain().Extent().width;
	tessFramebufferInfo.height = vulkan->Swapchain().Extent().height;
	tessFramebufferInfo.layers = 1;

	tessFramebuffers.resize(vulkan->Swapchain().ImageViews().size());
	for (size_t i = 0; i < vulkan->Swapchain().ImageViews().size(); i++)
	{
		tessAttachments[0] = vulkan->Swapchain().ImageViews()[i];
		tessAttachments[1] = tessVisibilityBuffer.visibility.ImageView();
		tessAttachments[2] = tessVisibilityBuffer.tessCoords_v1XYZ_v2X.ImageView();
		tessAttachments[3] = tessVisibilityBuffer.tessCoords_v2YZ_v3XY.ImageView();
		tessAttachments[4] = tessVisibilityBuffer.tessCoords_v3Z.ImageView();
		tessAttachments[5] = depthImage.ImageView();

		if (vkCreateFramebuffer(vulkan->Device(), &tessFramebufferInfo, nullptr, &tessFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create tessellation frame buffer");
		}
	}
}

void VulkanApplication::CreateFrameBufferAttachment(VkFormat format, VkImageUsageFlags usage, Image* attachment, VmaAllocator& allocator)
{
	// Create image
	attachment->Create(vulkan->Swapchain().Extent().width, vulkan->Swapchain().Extent().height, format, VK_IMAGE_TILING_OPTIMAL, usage, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

	// Create image view
	VkImageAspectFlags aspectMask = 0;
	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	attachment->CreateImageView(vulkan->Device(), aspectMask);
}

// Acquires image from swap chain
// Executes command buffer with that image as an attachment in the framebuffer
// Return image to swap chain for presentation
void VulkanApplication::DrawFrame()
{
	// Wait for previous frame to finish
	vkWaitForFences(vulkan->Device(), 1, &vulkan->Fences()[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	// Acquire image from swap chain. ImageAvailableSemaphore will be signaled when the image is ready to be drawn to. Check if we have to recreate the swap chain
	uint32_t imageIndex;
	VkSemaphore imageAvailableSemaphore = vulkan->ImageAvailableSemaphores()[currentFrame];
	VkSemaphore renderFinishedSemaphore = vulkan->RenderFinishedSemaphores()[currentFrame];
	VkResult result = vkAcquireNextImageKHR(vulkan->Device(), vulkan->Swapchain().VkHandle(), std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to acquire swap chain image");
	}

	// We reset fences here in the case that the swap chain needs rebuilding
	vkResetFences(vulkan->Device(), 1, &vulkan->Fences()[currentFrame]);

	// Update the uniform buffers
	UpdateUniformBuffers();

	// Submit the command buffer. Waits for the provided semaphores to be signaled before beginning execution
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // Waiting until we can start writing color, in theory this means the implementation can execute the vertex buffer while image isn't available
	submitInfo.pWaitDstStageMask = waitStages;

	// Wait for image to be available, and signal when rendering is finished
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

	// Update command buffer (for ImGui)
	RecordCommandBuffers();

	// Submit to queue
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.commandBufferCount = 1;
	if (vkQueueSubmit(vulkan->PhysDevice().Queues()->graphics, 1, &submitInfo, vulkan->Fences()[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit visBuff command buffer");
	}

	// Now submit the resulting image back to the swap chain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // Wait for imgui pass to finish

	VkSwapchainKHR swapChains[] = { vulkan->Swapchain().VkHandle() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	// Request to present image to the swap chain
	result = vkQueuePresentKHR(vulkan->PhysDevice().Queues()->present, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
	{
		framebufferResized = false;
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present swap chain image");
	}

	// Progress the current frame
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
#pragma endregion

#pragma region Command Buffer Functions
// Commands in vulkan must be defined in command buffers, so they can be set up in advance on multiple threads
// Command pools manage the memory that is used to store the command buffers.
void VulkanApplication::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = PhysicalDevice::FindQueueFamilies(vulkan->PhysDevice().VkHandle(), vulkan->Swapchain().Surface());

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allows command buffers to be reworked at runtime

	if (vkCreateCommandPool(vulkan->Device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

void VulkanApplication::AllocateCommandBuffers()
{
	commandBuffers.resize(vulkan->Swapchain().ImageViews().size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = SCAST_U32(commandBuffers.size());

	if (vkAllocateCommandBuffers(vulkan->Device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vis buffer command buffers");
	}
}

void VulkanApplication::RecordCommandBuffers()
{
	// Define clear values
	std::array<VkClearValue, 3> visBuffClearValues = {};
	visBuffClearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	visBuffClearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	visBuffClearValues[2].depthStencil = { 1.0f, 0 };
	std::array<VkClearValue, 6> tessClearValues = {};
	tessClearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	tessClearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	tessClearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	tessClearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	tessClearValues[4].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	tessClearValues[5].depthStencil = { 1.0f, 0 };

	// Begin recording command buffers
	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording vis buffer shade command buffer");
		}

		// Start the render pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = vulkan->Swapchain().Extent();
		if (currentPipeline == VISIBILITYBUFFER)
		{
			renderPassInfo.renderPass = visBuffRenderPass;
			renderPassInfo.framebuffer = visBuffFramebuffers[i];
			renderPassInfo.clearValueCount = SCAST_U32(visBuffClearValues.size());
			renderPassInfo.pClearValues = visBuffClearValues.data();
		}
		else
		{
			renderPassInfo.renderPass = tessRenderPass;
			renderPassInfo.framebuffer = tessFramebuffers[i];
			renderPassInfo.clearValueCount = SCAST_U32(tessClearValues.size());
			renderPassInfo.pClearValues = tessClearValues.data();
		}

		// Reset timestamp queries
		vkCmdResetQueryPool(commandBuffers[i], timestampPool, 0, 4);		

		// Begin the render pass
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = {};
		viewport.width = (float)vulkan->Swapchain().Extent().width;
		viewport.height = (float)vulkan->Swapchain().Extent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.extent = vulkan->Swapchain().Extent();
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);

		// First Subpass: Write to visibility buffer using one of two pipelines
		// -----------------------------------------
		// Record start timestamp
		vkCmdWriteTimestamp(commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 0);

		// Decide which pipeline to bind
		switch (currentPipeline)
		{
			case VISIBILITYBUFFER:
			{
				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffWritePipelineLayout, 0, 1, &visBuffWritePassDescSet, 0, nullptr);
				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffWritePipeline);
				VkDeviceSize offsets[1] = { 0 };
				VkBuffer vertexBuffers[] = { visBuffTerrain.VertexBuffer().VkHandle() };
				vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffers[i], visBuffTerrain.IndexBuffer().VkHandle(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffers[i], SCAST_U32(visBuffTerrain.Indices().size()), 1, 0, 0, 0);
				break;
			}
			case VB_TESSELLATION:
			{
				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, tessWritePipelineLayout, 0, 1, &tessWritePassDescSet, 0, nullptr);
				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, tessWritePipeline);
				VkDeviceSize offsets[1] = { 0 };
				VkBuffer vertexBuffers[] = { tessTerrain.VertexBuffer().VkHandle() };
				vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffers[i], tessTerrain.IndexBuffer().VkHandle(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffers[i], SCAST_U32(tessTerrain.Indices().size()), 1, 0, 0, 0);
				break;
			}
		}

		// Record end timestamp
		vkCmdWriteTimestamp(commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 1);
		// -----------------------------------------

		// Second Subpass: Shading Pass using one of two pipelines
		// -----------------------------------------
		vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

		// Record start timestamp
		vkCmdWriteTimestamp(commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 2);

		// Decide which pipeline to bind
		switch (currentPipeline)
		{
			case VISIBILITYBUFFER:
			{
				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffShadePipelineLayout, 0, 1, &visBuffShadePassDescSets[i], 0, nullptr);
				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffShadePipeline);
				vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); // Vertex shader calculates positions of fullscreen triangle based on index, so no need to bind vertex/index buffers to create a fullscreen quad
				break;
			}
			case VB_TESSELLATION:
			{
				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, tessShadePipelineLayout, 0, 1, &tessShadePassDescSets[i], 0, nullptr);
				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, tessShadePipeline);
				vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); // Vertex shader calculates positions of fullscreen triangle based on index, so no need to bind vertex/index buffers to create a fullscreen quad
				break;
			}
		}

		// Record end timestamp
		vkCmdWriteTimestamp(commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 3);
		// -----------------------------------------

#if IMGUI_ENABLED
		// Imgui pass
		imGui.DrawFrame(commandBuffers[i]);
#endif

		// Now end the render pass
		vkCmdEndRenderPass(commandBuffers[i]);

		// And end recording of command buffers
		if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record vis Buff Shade command buffer");
		}
	}
}
#pragma endregion

#pragma region Depth Buffer Functions
void VulkanApplication::CreateDepthResources()
{
	// Select format
	VkFormat depthFormat = FindDepthFormat();

	// Create Image and ImageView objects
	depthImage.Create(vulkan->Swapchain().Extent().width, vulkan->Swapchain().Extent().height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);
	depthImage.CreateImageView(vulkan->Device(), VK_IMAGE_ASPECT_DEPTH_BIT);

	// Transition depth image for shader usage
	depthImage.TransitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, vulkan->Device(), vulkan->PhysDevice(), commandPool);
}

VkFormat VulkanApplication::FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

// Checks a list of candidates ordered from most to least desirable and returns the first supported format
VkFormat VulkanApplication::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(vulkan->PhysDevice().VkHandle(), format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}
#pragma endregion

#pragma region Buffer Functions
void VulkanApplication::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(MVPUniformBufferObject);

	// Create uniform buffer terrain transform matrices
	mvpUniformBuffer.Create(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

	bufferSize = sizeof(SettingsUBO);

	// Create tess factor UBO for tessellation pipeline
	settingsBuffer.Create(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);
}

void VulkanApplication::UpdateUniformBuffers()
{
	// Get time since rendering started
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	// Now generate the model, view and projection matrices
	glm::mat4 modelMatrix = glm::mat4(1.0f);// glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 viewMatrix = camera.ViewMatrix();
	glm::mat4 projMatrix = camera.ProjectionMatrix();
	projMatrix[1][1] *= -1; // Flip Y of projection matrix to account for OpenGL's flipped Y clip axis
	glm::mat4 inverseViewProj = glm::inverse((projMatrix * viewMatrix));

	// Fill MVP Uniform Buffer
	MVPUniformBufferObject ubo = {};
	ubo.mvp = (projMatrix * viewMatrix) * modelMatrix;
	ubo.proj = projMatrix;
	//ubo.invViewProj = inverseViewProj;

	// Now map the memory to mvp uniform buffer
	mvpUniformBuffer.MapData(&ubo, allocator);

	// Map rendering settings to ubo
	settingsBuffer.MapData(&renderSettingsUbo, allocator);

	// Update light ubo
	light.UpdateUBO(allocator);
}

void VulkanApplication::CreateVmaAllocator()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = vulkan->PhysDevice().VkHandle();
	allocatorInfo.device = vulkan->Device();
	vmaCreateAllocator(&allocatorInfo, &allocator);
}
#pragma endregion

#pragma region Descriptor Functions
void VulkanApplication::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 4> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = (SCAST_U32(vulkan->Swapchain().Images().size()) * 6) + 3; // mvp UBO, light UBO and settings UBO per swapchain image plus mvp ubo for the write pass plus mvp ubo and settings for tess write pass
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = (SCAST_U32(vulkan->Swapchain().Images().size()) * 6) + 2; // terrain texture and heightmap and normalmap per swapchain image per pipeline plus two for the write pipelines
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[2].descriptorCount = (SCAST_U32(vulkan->Swapchain().Images().size()) * 2) * 2; // 2 storage buffers per swapchain image per shade pass
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	poolSizes[3].descriptorCount = SCAST_U32(vulkan->Swapchain().Images().size()) * 5; // 5 input attachment per swapchain image (vis buff + tessvisbuff + 3 tesscoords)

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = SCAST_U32(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = (SCAST_U32(vulkan->Swapchain().Images().size()) * 2) + 2; // 2 descriptor set per swapchain image, one for the write pass and one for the tess write pass

	if (vkCreateDescriptorPool(vulkan->Device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool");
	}
}

void VulkanApplication::CreateShadePassDescriptorSetLayouts()
{
	// Descriptor layouts for the shading passes of both pipelines
	// Binding 0: Model texture sampler
	VkDescriptorSetLayoutBinding textureSamplerBinding = {};
	textureSamplerBinding.binding = 0;
	textureSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureSamplerBinding.descriptorCount = 1;
	textureSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 1: Visibility Buffer
	VkDescriptorSetLayoutBinding visBufferBinding = {};
	visBufferBinding.binding = 1;
	visBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	visBufferBinding.descriptorCount = 1;
	visBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 2: MVP Uniform Buffer
	VkDescriptorSetLayoutBinding modelUboLayoutBinding = {};
	modelUboLayoutBinding.binding = 2;
	modelUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	modelUboLayoutBinding.descriptorCount = 1;
	modelUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; 

	// Binding 3: Index Buffer
	VkDescriptorSetLayoutBinding indexBufferBinding = {};
	indexBufferBinding.binding = 3;
	indexBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indexBufferBinding.descriptorCount = 1;
	indexBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 4: Vertex Attribute Buffer
	VkDescriptorSetLayoutBinding attributeBufferBinding = {};
	attributeBufferBinding.binding = 4;
	attributeBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	attributeBufferBinding.descriptorCount = 1;
	attributeBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 5: Render Settings Buffer
	VkDescriptorSetLayoutBinding settingsBufferBinding = {};
	settingsBufferBinding.binding = 5;
	settingsBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	settingsBufferBinding.descriptorCount = 1;
	settingsBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 6: Heightmap texture sampler
	VkDescriptorSetLayoutBinding heightmapLayoutBinding = {};
	heightmapLayoutBinding.binding = 6;
	heightmapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	heightmapLayoutBinding.descriptorCount = 1;
	heightmapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 7: Normalmap texture sampler
	VkDescriptorSetLayoutBinding normalmapLayoutBinding = {};
	normalmapLayoutBinding.binding = 7;
	normalmapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalmapLayoutBinding.descriptorCount = 1;
	normalmapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 8: Directional light UBO
	VkDescriptorSetLayoutBinding lightUboBinding = {};
	lightUboBinding.binding = 8;
	lightUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightUboBinding.descriptorCount = 1;
	lightUboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 9: TessCoords Buffer 1 (Tessellation pipeline only)
	VkDescriptorSetLayoutBinding tessBufferBinding1 = {};
	tessBufferBinding1.binding = 9;
	tessBufferBinding1.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	tessBufferBinding1.descriptorCount = 1;
	tessBufferBinding1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 10: TessCoords Buffer 2 (Tessellation pipeline only)
	VkDescriptorSetLayoutBinding tessBufferBinding2 = {};
	tessBufferBinding2.binding = 10;
	tessBufferBinding2.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	tessBufferBinding2.descriptorCount = 1;
	tessBufferBinding2.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 11: TessCoords Buffer 3 (Tessellation pipeline only)
	VkDescriptorSetLayoutBinding tessBufferBinding3 = {};
	tessBufferBinding3.binding = 11;
	tessBufferBinding3.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	tessBufferBinding3.descriptorCount = 1;
	tessBufferBinding3.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Create descriptor set layout for Visibility Buffer Pipeline
	std::array<VkDescriptorSetLayoutBinding, 9> visBuffBindings = { modelUboLayoutBinding, textureSamplerBinding, visBufferBinding, indexBufferBinding, attributeBufferBinding, settingsBufferBinding, heightmapLayoutBinding, normalmapLayoutBinding, lightUboBinding };
	VkDescriptorSetLayoutCreateInfo visBuffLayoutInfo = {};
	visBuffLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	visBuffLayoutInfo.bindingCount = SCAST_U32(visBuffBindings.size());
	visBuffLayoutInfo.pBindings = visBuffBindings.data();
	if (vkCreateDescriptorSetLayout(vulkan->Device(), &visBuffLayoutInfo, nullptr, &visBuffShadePassDescSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buffer shade pass descriptor set layout");
	}

	// Create descriptor set layout for Tessellation Pipeline
	std::array<VkDescriptorSetLayoutBinding, 12> tessBindings = { modelUboLayoutBinding, textureSamplerBinding, visBufferBinding, indexBufferBinding, attributeBufferBinding, settingsBufferBinding, heightmapLayoutBinding, normalmapLayoutBinding, lightUboBinding, tessBufferBinding1, tessBufferBinding2, tessBufferBinding3 };
	VkDescriptorSetLayoutCreateInfo tessLayoutInfo = {};
	tessLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	tessLayoutInfo.bindingCount = SCAST_U32(tessBindings.size());
	tessLayoutInfo.pBindings = tessBindings.data();
	if (vkCreateDescriptorSetLayout(vulkan->Device(), &tessLayoutInfo, nullptr, &tessShadePassDescSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create tessellation shade pass descriptor set layout");
	}
}

void VulkanApplication::CreateVisBuffWritePassDescriptorSetLayout()
{
	// Descriptor layout for the write pass
	// Binding 0: Vertex Shader Uniform Buffer of loaded model
	VkDescriptorSetLayoutBinding modelUboLayoutBinding = {};
	modelUboLayoutBinding.binding = 0;
	modelUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	modelUboLayoutBinding.descriptorCount = 1;
	modelUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Specify that this descriptor will be used in the vertex shader

	// Binding 1: Heightmap texture sampler
	VkDescriptorSetLayoutBinding heightmapLayoutBinding = {};
	heightmapLayoutBinding.binding = 1;
	heightmapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	heightmapLayoutBinding.descriptorCount = 1;
	heightmapLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Create descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { modelUboLayoutBinding, heightmapLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = SCAST_U32(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(vulkan->Device(), &layoutInfo, nullptr, &visBuffWritePassDescSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create write pass descriptor set layout");
	}
}

void VulkanApplication::CreateTessWritePassDescriptorSetLayout()
{
	// Descriptor layout for the tessellation write pass
	// Binding 0: Render settings UBO for hull shader tess factor
	VkDescriptorSetLayoutBinding tessFactorLayoutBinding = {};
	tessFactorLayoutBinding.binding = 0;
	tessFactorLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	tessFactorLayoutBinding.descriptorCount = 1;
	tessFactorLayoutBinding.stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; // Specify that this descriptor will be used in the hull shader

	// Binding 1: Domain Shader MVP Buffer of terrain
	VkDescriptorSetLayoutBinding modelUboLayoutBinding = {};
	modelUboLayoutBinding.binding = 1;
	modelUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	modelUboLayoutBinding.descriptorCount = 1;
	modelUboLayoutBinding.stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; // Specify that this descriptor will be used in the domain shader

	// Binding 2: Heightmap texture sampler
	VkDescriptorSetLayoutBinding heightmapLayoutBinding = {};
	heightmapLayoutBinding.binding = 2;
	heightmapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	heightmapLayoutBinding.descriptorCount = 1;
	heightmapLayoutBinding.stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

	// Create descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 3> bindings = { tessFactorLayoutBinding, modelUboLayoutBinding, heightmapLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = SCAST_U32(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(vulkan->Device(), &layoutInfo, nullptr, &tessWritePassDescSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create tess write pass descriptor set layout");
	}
}

// Create the descriptor sets for the shade pass, containing the visibility buffer images (for each swapchain image)
void VulkanApplication::CreateShadePassDescriptorSets()
{
	// Create shade descriptor sets
	std::vector<VkDescriptorSetLayout> visBuffShadingLayouts(vulkan->Swapchain().Images().size(), visBuffShadePassDescSetLayout);
	std::vector<VkDescriptorSetLayout> tessShadingLayouts(vulkan->Swapchain().Images().size(), tessShadePassDescSetLayout);

	// Allocate vis buff shade pass descriptor set
	VkDescriptorSetAllocateInfo shadePassAllocInfo = {};
	shadePassAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	shadePassAllocInfo.descriptorPool = descriptorPool;
	shadePassAllocInfo.descriptorSetCount = SCAST_U32(vulkan->Swapchain().Images().size());
	shadePassAllocInfo.pSetLayouts = visBuffShadingLayouts.data();
	visBuffShadePassDescSets.resize(vulkan->Swapchain().Images().size());
	if (vkAllocateDescriptorSets(vulkan->Device(), &shadePassAllocInfo, visBuffShadePassDescSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vis buff shade pass descriptor sets");
	}

	// Allocate tess shade pass descriptor set
	shadePassAllocInfo.pSetLayouts = tessShadingLayouts.data();
	tessShadePassDescSets.resize(vulkan->Swapchain().Images().size());
	if (vkAllocateDescriptorSets(vulkan->Device(), &shadePassAllocInfo, tessShadePassDescSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate tess shade pass descriptor sets");
	}

	// Configure the descriptors
	for (size_t i = 0; i < vulkan->Swapchain().Images().size(); i++)
	{
		// Terrain texture sampler
		visBuffTerrain.SetupTextureDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, visBuffShadePassDescSets[i], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

		// Visibility Buffer attachment
		visibilityBuffer.visibility.SetUpDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);
		visibilityBuffer.visibility.SetupDescriptorWriteSet(visBuffShadePassDescSets[i], 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1);

		// Terrain UBO
		mvpUniformBuffer.SetupDescriptor(sizeof(MVPUniformBufferObject), 0);
		mvpUniformBuffer.SetupDescriptorWriteSet(visBuffShadePassDescSets[i], 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

		// Terrain buffers
		visBuffTerrain.SetupIndexBufferDescriptor(visBuffShadePassDescSets[i], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
		visBuffTerrain.SetupAttributeBufferDescriptor(visBuffShadePassDescSets[i], 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);

		// Settings UBO
		settingsBuffer.SetupDescriptor(sizeof(SettingsUBO), 0);
		settingsBuffer.SetupDescriptorWriteSet(visBuffShadePassDescSets[i], 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

		// Heightmap texture
		visBuffTerrain.SetupHeightmapDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, visBuffShadePassDescSets[i], 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

		// Normalmap texture
		visBuffTerrain.SetupNormalmapDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, visBuffShadePassDescSets[i], 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

		// Directional light ubo
		light.SetupUBODescriptors(visBuffShadePassDescSets[i], 8, 1);

		// Create a descriptor writes for each descriptor in the set
		std::array<VkWriteDescriptorSet, 9> visBuffShadePassDescriptorWrites = {};
		visBuffShadePassDescriptorWrites[0] = visBuffTerrain.GetTexture().WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[1] = visibilityBuffer.visibility.WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[2] = mvpUniformBuffer.WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[3] = visBuffTerrain.IndexBuffer().WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[4] = visBuffTerrain.AttributeBuffer().WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[5] = settingsBuffer.WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[6] = visBuffTerrain.Heightmap().WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[7] = visBuffTerrain.Normalmap().WriteDescriptorSet();
		visBuffShadePassDescriptorWrites[8] = light.UBO().WriteDescriptorSet();
		vkUpdateDescriptorSets(vulkan->Device(), SCAST_U32(visBuffShadePassDescriptorWrites.size()), visBuffShadePassDescriptorWrites.data(), 0, nullptr);

		// Now for the tessellation pipeline
		tessTerrain.SetupTextureDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tessShadePassDescSets[i], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
		mvpUniformBuffer.SetupDescriptorWriteSet(tessShadePassDescSets[i], 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
		tessTerrain.SetupIndexBufferDescriptor(tessShadePassDescSets[i], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
		tessTerrain.SetupAttributeBufferDescriptor(tessShadePassDescSets[i], 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
		settingsBuffer.SetupDescriptorWriteSet(tessShadePassDescSets[i], 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
		visBuffTerrain.SetupHeightmapDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tessShadePassDescSets[i], 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
		visBuffTerrain.SetupNormalmapDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tessShadePassDescSets[i], 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
		light.SetupUBODescriptors(tessShadePassDescSets[i], 8, 1);
		// Tess Visibility Buffer attachment
		tessVisibilityBuffer.visibility.SetUpDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);
		tessVisibilityBuffer.visibility.SetupDescriptorWriteSet(tessShadePassDescSets[i], 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1);
		tessVisibilityBuffer.tessCoords_v1XYZ_v2X.SetUpDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);
		tessVisibilityBuffer.tessCoords_v1XYZ_v2X.SetupDescriptorWriteSet(tessShadePassDescSets[i], 9, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1);
		tessVisibilityBuffer.tessCoords_v2YZ_v3XY.SetUpDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);
		tessVisibilityBuffer.tessCoords_v2YZ_v3XY.SetupDescriptorWriteSet(tessShadePassDescSets[i], 10, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1);
		tessVisibilityBuffer.tessCoords_v3Z.SetUpDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);
		tessVisibilityBuffer.tessCoords_v3Z.SetupDescriptorWriteSet(tessShadePassDescSets[i], 11, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1);

		std::array<VkWriteDescriptorSet, 12> tessShadePassDescriptorWrites = {};
		tessShadePassDescriptorWrites[0] = tessTerrain.GetTexture().WriteDescriptorSet();
		tessShadePassDescriptorWrites[1] = tessVisibilityBuffer.visibility.WriteDescriptorSet();
		tessShadePassDescriptorWrites[2] = mvpUniformBuffer.WriteDescriptorSet();
		tessShadePassDescriptorWrites[3] = tessTerrain.IndexBuffer().WriteDescriptorSet();
		tessShadePassDescriptorWrites[4] = tessTerrain.AttributeBuffer().WriteDescriptorSet();
		tessShadePassDescriptorWrites[5] = settingsBuffer.WriteDescriptorSet();
		tessShadePassDescriptorWrites[6] = visBuffTerrain.Heightmap().WriteDescriptorSet();
		tessShadePassDescriptorWrites[7] = visBuffTerrain.Normalmap().WriteDescriptorSet();
		tessShadePassDescriptorWrites[8] = light.UBO().WriteDescriptorSet();
		tessShadePassDescriptorWrites[9] = tessVisibilityBuffer.tessCoords_v1XYZ_v2X.WriteDescriptorSet();
		tessShadePassDescriptorWrites[10] = tessVisibilityBuffer.tessCoords_v2YZ_v3XY.WriteDescriptorSet();
		tessShadePassDescriptorWrites[11] = tessVisibilityBuffer.tessCoords_v3Z.WriteDescriptorSet();
		vkUpdateDescriptorSets(vulkan->Device(), SCAST_U32(tessShadePassDescriptorWrites.size()), tessShadePassDescriptorWrites.data(), 0, nullptr);
	}
}

// Create the descriptor sets for the write pass, containing the MVP uniform buffer and heightmap
void VulkanApplication::CreateWritePassDescriptorSet()
{
	// Create write pass descriptor sets
	std::vector<VkDescriptorSetLayout> writeLayouts = { visBuffWritePassDescSetLayout };

	// Create write pass descriptor set for MVP UBO and heightmap texture
	VkDescriptorSetAllocateInfo writePassAllocInfo = {};
	writePassAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	writePassAllocInfo.descriptorPool = descriptorPool;
	writePassAllocInfo.descriptorSetCount = 1;
	writePassAllocInfo.pSetLayouts = writeLayouts.data();

	if (vkAllocateDescriptorSets(vulkan->Device(), &writePassAllocInfo, &visBuffWritePassDescSet) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate write pass descriptor sets");
	}

	// Terrain UBO
	mvpUniformBuffer.SetupDescriptor(sizeof(MVPUniformBufferObject), 0);
	mvpUniformBuffer.SetupDescriptorWriteSet(visBuffWritePassDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

	// Heightmap texture
	visBuffTerrain.SetupHeightmapDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, visBuffWritePassDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

	// Create a descriptor write for each descriptor in the set
	std::array<VkWriteDescriptorSet, 2> writePassDescriptorWrites = {};

	// Binding 0: MVP Uniform Buffer of terrain
	writePassDescriptorWrites[0] = mvpUniformBuffer.WriteDescriptorSet();

	// Binding 1: Heightmap texture
	writePassDescriptorWrites[1] = visBuffTerrain.Heightmap().WriteDescriptorSet();

	vkUpdateDescriptorSets(vulkan->Device(), SCAST_U32(writePassDescriptorWrites.size()), writePassDescriptorWrites.data(), 0, nullptr);
}

// Create the descriptor sets for the tessellation write pass, containing the MVP uniform buffer, heightmap and tessellation factors
void VulkanApplication::CreateTessWritePassDescriptorSet()
{
	// Create write pass descriptor sets
	std::vector<VkDescriptorSetLayout> tessWriteLayouts = { tessWritePassDescSetLayout };

	// Create write pass descriptor set for MVP UBO and heightmap texture
	VkDescriptorSetAllocateInfo tessWritePassAllocInfo = {};
	tessWritePassAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	tessWritePassAllocInfo.descriptorPool = descriptorPool;
	tessWritePassAllocInfo.descriptorSetCount = 1;
	tessWritePassAllocInfo.pSetLayouts = tessWriteLayouts.data();

	if (vkAllocateDescriptorSets(vulkan->Device(), &tessWritePassAllocInfo, &tessWritePassDescSet) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate tess write pass descriptor sets");
	}

	// Settings UBO
	settingsBuffer.SetupDescriptor(sizeof(SettingsUBO), 0);
	settingsBuffer.SetupDescriptorWriteSet(tessWritePassDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

	// Terrain UBO
	mvpUniformBuffer.SetupDescriptor(sizeof(MVPUniformBufferObject), 0);
	mvpUniformBuffer.SetupDescriptorWriteSet(tessWritePassDescSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

	// Heightmap texture
	visBuffTerrain.SetupHeightmapDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tessWritePassDescSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

	// Create a descriptor write for each descriptor in the set
	std::array<VkWriteDescriptorSet, 3> tessWritePassDescriptorWrites = {};

	// Binding 0: Rendering settings
	tessWritePassDescriptorWrites[0] = settingsBuffer.WriteDescriptorSet();

	// Binding 1: MVP Uniform Buffer of terrain
	tessWritePassDescriptorWrites[1] = mvpUniformBuffer.WriteDescriptorSet();

	// Binding 2: Heightmap texture
	tessWritePassDescriptorWrites[2] = visBuffTerrain.Heightmap().WriteDescriptorSet();

	vkUpdateDescriptorSets(vulkan->Device(), SCAST_U32(tessWritePassDescriptorWrites.size()), tessWritePassDescriptorWrites.data(), 0, nullptr);
}
#pragma endregion

#pragma region Other Functions
void VulkanApplication::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}
#pragma endregion 

} // namespace