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
	window = glfwCreateWindow(WIDTH, HEIGHT, "Visibility Buffer", nullptr, nullptr);
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
	CreateCommandPool();
	CreateVisBuffWriteRenderPass();
	CreateVisBuffShadeRenderPass();
	CreateShadePassDescriptorSetLayout();
	CreateWritePassDescriptorSetLayout();
	CreatePipelineCache();
	CreateVisBuffWritePipeline();
	CreateVisBuffShadePipeline();
	terrainTexture.Create(TEXTURE_PATH, allocator, vulkan->Device(), vulkan->PhysDevice(), commandPool);
	CreateDepthSampler();
	//chalet.LoadFromFile(MODEL_PATH);
	terrain.Generate(glm::vec3(0.0f), 1.0f, 1.0f);
	CreateVertexBuffer();
	CreateAttributeBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateFrameBuffers();
	CreateShadePassDescriptorSets();
	CreateWritePassDescriptorSet();
	RecordVisBuffShadeCommandBuffers();
	RecordVisBuffWriteCommandBuffer();
}

void VulkanApplication::Update()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		UpdateMouse();
		
		// Draw frame and calculate frame time
		auto frameStart = std::chrono::high_resolution_clock::now();
		DrawFrame();
		auto frameEnd = std::chrono::high_resolution_clock::now();
		auto diff = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
		frameTime = (float)diff / 1000.0f;

		camera.Update(frameTime);
	}

	// Wait for the device to finish up any operations when exiting the main loop
	vkDeviceWaitIdle(vulkan->Device());
}

void VulkanApplication::CleanUp()
{
	CleanUpSwapChain();

	// Destroy texture objects
	terrainTexture.CleanUp(allocator, vulkan->Device());
	vkDestroySampler(vulkan->Device(), depthSampler, nullptr);
	visibilityBuffer.visibility.CleanUp(allocator, vulkan->Device());
	debugAttachment.CleanUp(allocator, vulkan->Device());

	// Destroy Descriptor Pool
	vkDestroyDescriptorPool(vulkan->Device(), descriptorPool, nullptr);

	// Destroy descriptor layouts
	vkDestroyDescriptorSetLayout(vulkan->Device(), shadePassDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkan->Device(), writePassDescriptorSetLayout, nullptr);

	// Destroy uniform buffers
	mvpUniformBuffer.CleanUp(allocator);

	// Destroy vertex and index buffers
	vertexBuffer.CleanUp(allocator);
	indexBuffer.CleanUp(allocator);
	vertexAttributeBuffer.CleanUp(allocator);
	vmaDestroyAllocator(allocator);

	// Destroy geometry pass semaphore
	vkDestroySemaphore(vulkan->Device(), visBuffWriteSemaphore, nullptr);

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

#pragma region Input Functions
// Callback used by glfw when keyboard or mouse input is recieved
void VulkanApplication::ProcessKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Get vulkan app instance from window user pointer
	void *data = glfwGetWindowUserPointer(window);
	VulkanApplication* vulkanApp = static_cast<VulkanApplication*>(data);

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
}

void VulkanApplication::UpdateMouse()
{
	double xPos, yPos;
	glfwGetCursorPos(window, &xPos, &yPos);
	int deltaX = (int)mousePosition.x - (int)xPos;
	int deltaY = (int)mousePosition.y - (int)yPos; 
	
	if (mouseLeftDown)
	{
		camera.Rotate(glm::vec3(-deltaY * camera.rotateSpeed, -deltaX * camera.rotateSpeed, 0.0f));
	}

	mousePosition = { xPos, yPos };
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

	CleanUpSwapChain();

	// Recreate required objects
	vulkan->Swapchain().RecreateSwapChain(window, vulkan->PhysDevice().VkHandle(), vulkan->Device());
	CreateVisBuffWriteRenderPass();
	CreateVisBuffShadeRenderPass();
	CreatePipelineCache();
	CreateVisBuffWritePipeline();
	CreateVisBuffShadePipeline();
	CreateDepthResources();
	CreateFrameBuffers();
	RecordVisBuffShadeCommandBuffers();
	RecordVisBuffWriteCommandBuffer();
}

void VulkanApplication::CleanUpSwapChain()
{
	// Destroy depth buffers
	visibilityBuffer.depth.CleanUp(allocator, vulkan->Device());
	visBuffShadeDepthImage.CleanUp(allocator, vulkan->Device());

	// Destroy frame buffers
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
	{
		vkDestroyFramebuffer(vulkan->Device(), swapChainFramebuffers[i], nullptr);
	}
	vkDestroyFramebuffer(vulkan->Device(), visibilityBuffer.frameBuffer, nullptr);

	// Free command buffers
	vkFreeCommandBuffers(vulkan->Device(), commandPool, SCAST_U32(visBuffShadeCommandBuffers.size()), visBuffShadeCommandBuffers.data());
	vkFreeCommandBuffers(vulkan->Device(), commandPool, 1, &visBuffWriteCommandBuffer);
	vkDestroyPipeline(vulkan->Device(), visBuffShadePipeline, nullptr);
	vkDestroyPipeline(vulkan->Device(), visBuffWritePipeline, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), visBuffShadePipelineLayout, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), visBuffWritePipelineLayout, nullptr);
	vkDestroyPipelineCache(vulkan->Device(), pipelineCache, nullptr);
	vkDestroyRenderPass(vulkan->Device(), visBuffShadeRenderPass, nullptr);
	vkDestroyRenderPass(vulkan->Device(), visBuffWriteRenderPass, nullptr);
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
void VulkanApplication::CreateVisBuffShadePipeline()
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
	VkPipelineShaderStageCreateInfo visBuffShadeShaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };	

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
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

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

	// Set up color blending (disbaled, all fragment colors will go to the framebuffer unmodified)
	VkPipelineColorBlendAttachmentState colourBlendAttachment = {};
	colourBlendAttachment.colorWriteMask = 0xf;
	colourBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendAttachmentState debugBlendAttachment = {};
	debugBlendAttachment.colorWriteMask = 0xf;
	debugBlendAttachment.blendEnable = VK_FALSE;
	std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments = { colourBlendAttachment, debugBlendAttachment };
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

	// PipelineLayout
	CreateVisBuffShadePipelineLayout();

	// We now have everything we need to create the vis buff shade graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = visBuffShadeShaderStages;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colourBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = visBuffShadePipelineLayout;
	pipelineInfo.renderPass = visBuffShadeRenderPass;
	pipelineInfo.subpass = 0; // Index of the sub pass where this pipeline will be used
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
}

void VulkanApplication::CreateVisBuffWritePipeline()
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
	rasterizer.polygonMode = WIREFRAME ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Cull back faces
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Faces are drawn counter clockwise to be considered front-facing
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// Set up depth test
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	// Set up multisampling (disabled)
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// We need to set up color blend attachments for all of the visibility buffer color attachments in the subpass (just vis buff atm)
	VkPipelineColorBlendAttachmentState visBlendAttachment = {};
	visBlendAttachment.colorWriteMask = 0xf;
	visBlendAttachment.blendEnable = VK_FALSE;
	std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachments = { visBlendAttachment };
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

	// PipelineLayout
	CreateVisBuffWritePipelineLayout();

	// We now have everything we need to create the vis buff write graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.layout = visBuffWritePipelineLayout;
	pipelineInfo.renderPass = visBuffWriteRenderPass;
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
}

void VulkanApplication::CreateVisBuffShadePipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &shadePassDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;

	// Vis Buff Shade Layout
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &visBuffShadePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff shade pipeline layout");
	}
}

void VulkanApplication::CreateVisBuffWritePipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &writePassDescriptorSetLayout; 
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;

	// Geometry layout
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &visBuffWritePipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create geometry pipeline layout");
	}
}

void VulkanApplication::CreateVisBuffWriteRenderPass()
{
	// Create Frame Buffer attachments
	CreateFrameBufferAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &visibilityBuffer.visibility, allocator); // 32 bit uint will be unpacked into four 8bit floats
	CreateDepthResources();

	// Create attachment descriptions for the visibility buffer
	std::array<VkAttachmentDescription, 2> attachments = {};

	// Fill attachment properties
	for (uint32_t i = 0; i < 2; ++i)
	{
		attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 1) // Depth attachment
		{
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}
	// Fill Formats
	attachments[0].format = visibilityBuffer.visibility.Format();
	attachments[1].format = visibilityBuffer.depth.Format();

	// Create attachment references for the subpass to use
	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	// Create subpass (one for now)
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Specify that this is a graphics subpass, not compute
	subpass.colorAttachmentCount = SCAST_U32(colorReferences.size());;
	subpass.pColorAttachments = colorReferences.data(); // Important: The attachment's index in this array is what is referenced in the out variable of the shader!
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for attachment layout transitions
	std::array<VkSubpassDependency, 2> dependencies = {};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Create the render pass with required attachments
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = SCAST_U32(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();

	// Create geometry pass
	if (vkCreateRenderPass(vulkan->Device(), &renderPassInfo, nullptr, &visBuffWriteRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass");
	}
}

void VulkanApplication::CreateVisBuffShadeRenderPass()
{
	// Create debug attachment
	CreateFrameBufferAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &debugAttachment, allocator);

	std::array<VkAttachmentDescription, 3> attachments = {};
	// Color attachment
	attachments[0].format = vulkan->Swapchain().ImageFormat();
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Debug Attachment
	attachments[1].format = debugAttachment.Format();
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	// Depth attachment
	attachments[2].format = FindDepthFormat();
	attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Attachment References
	VkAttachmentReference colourReference = {};
	colourReference.attachment = 0;
	colourReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference debugReference = {};
	debugReference.attachment = 1;
	debugReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthReference = {};
	depthReference.attachment = 2;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	std::vector<VkAttachmentReference> colourReferences = { colourReference, debugReference };

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = SCAST_U32(colourReferences.size());
	subpassDescription.pColorAttachments = colourReferences.data();
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0; /*3;*/
	subpassDescription.pInputAttachments = nullptr;/* inputAttachmentRefs;*/
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies = {};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = SCAST_U32(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = SCAST_U32(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	// Create vis buff shade render pass
	if (vkCreateRenderPass(vulkan->Device(), &renderPassInfo, nullptr, &visBuffShadeRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff shade render pass");
	}
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
	camera.SetPerspective(45.0f, (float)vulkan->Swapchain().Extent().width / (float)vulkan->Swapchain().Extent().height, 0.1f, 100.0f, true);
	camera.SetPosition(glm::vec3(0.0f, 0.0f, -2.0f), true);
	camera.SetRotation(glm::vec3(0.0f, 0.0f, 0.0f), true);
}

void VulkanApplication::CreateFrameBuffers()
{
	// Create Visibility Buffer frame buffer
	std::array<VkImageView, 2> attachments = {};
	attachments[0] = visibilityBuffer.visibility.ImageView();
	attachments[1] = visibilityBuffer.depth.ImageView();

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.pNext = NULL;
	framebufferInfo.renderPass = visBuffWriteRenderPass; // Tell frame buffer which render pass it should be compatible with
	framebufferInfo.attachmentCount = SCAST_U32(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = vulkan->Swapchain().Extent().width;
	framebufferInfo.height = vulkan->Swapchain().Extent().height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(vulkan->Device(), &framebufferInfo, nullptr, &visibilityBuffer.frameBuffer) != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to create frame buffer");
	}

	// Create visibility buffer shade frame buffer for each swapchain image
	swapChainFramebuffers.resize(vulkan->Swapchain().ImageViews().size());
	for (size_t i = 0; i < vulkan->Swapchain().ImageViews().size(); i++)
	{
		std::array<VkImageView, 3> attachments =
		{
			vulkan->Swapchain().ImageViews()[i],
			debugAttachment.ImageView(),
			visBuffShadeDepthImage.ImageView()
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.pNext = NULL;
		framebufferInfo.renderPass = visBuffShadeRenderPass; // Tell frame buffer which render pass it should be compatible with
		framebufferInfo.attachmentCount = SCAST_U32(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = vulkan->Swapchain().Extent().width;
		framebufferInfo.height = vulkan->Swapchain().Extent().height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vulkan->Device(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to create frame buffer");
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
	UpdateMVPUniformBuffer();

	/// VIS WRITE PASS =======================================================================================
	// Submit the command buffer. Waits for the provided semaphores to be signaled before beginning execution
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // Waiting until we can start writing color, in theory this means the implementation can execute the vertex buffer while image isn't available
	submitInfo.pWaitDstStageMask = waitStages;

	// Wait for image to be available, and signal when visibility buffer is filled
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &visBuffWriteSemaphore;

	// Submit to queue
	submitInfo.pCommandBuffers = &visBuffWriteCommandBuffer;
	submitInfo.commandBufferCount = 1;
	if (vkQueueSubmit(vulkan->PhysDevice().Queues()->graphics, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit visBuffWrite command buffer");
	}
	/// =====================================================================================================

	/// VIS SHADE PASS =======================================================================================
	// Wait for g-buffer being filled, signal when rendering is complete
	VkSemaphore renderFinishedSemaphore = vulkan->RenderFinishedSemaphores()[currentFrame];
	submitInfo.pWaitSemaphores = &visBuffWriteSemaphore;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

	// Submit to queue
	submitInfo.pCommandBuffers = &visBuffShadeCommandBuffers[imageIndex];
	if (vkQueueSubmit(vulkan->PhysDevice().Queues()->graphics, 1, &submitInfo, vulkan->Fences()[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit visBuffShade command buffer");
	}
	/// =====================================================================================================

	// Now submit the resulting image back to the swap chain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // Wait for shade pass to finish

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
	poolInfo.flags = 0; // Optional. Allows command buffers to be reworked at runtime

	if (vkCreateCommandPool(vulkan->Device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

void VulkanApplication::RecordVisBuffShadeCommandBuffers()
{
	visBuffShadeCommandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = SCAST_U32(visBuffShadeCommandBuffers.size());

	if (vkAllocateCommandBuffers(vulkan->Device(), &allocInfo, visBuffShadeCommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vis buffer shade command buffers");
	}

	// Define clear values
	std::array<VkClearValue, 3> clearValues = {};
	clearValues[0].color = CLEAR_COLOUR;
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].depthStencil = { 1.0f, 0 };

	// Begin recording command buffers
	for (size_t i = 0; i < visBuffShadeCommandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(visBuffShadeCommandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording vis buffer shade command buffer");
		}

		// Start the render pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = visBuffShadeRenderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = vulkan->Swapchain().Extent();
		renderPassInfo.clearValueCount = SCAST_U32(clearValues.size());;
		renderPassInfo.pClearValues = clearValues.data();

		// The first parameter for every command is always the command buffer to record the command to.
		vkCmdBeginRenderPass(visBuffShadeCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = {};
		viewport.width = (float)vulkan->Swapchain().Extent().width;
		viewport.height = (float)vulkan->Swapchain().Extent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(visBuffShadeCommandBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.extent = vulkan->Swapchain().Extent();
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(visBuffShadeCommandBuffers[i], 0, 1, &scissor);

		// Bind the correct descriptor set for this swapchain image
		vkCmdBindDescriptorSets(visBuffShadeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffShadePipelineLayout, 0, 1, &shadePassDescriptorSets[i], 0, nullptr);

		// Now bind the graphics pipeline
		vkCmdBindPipeline(visBuffShadeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffShadePipeline);

		// Vertex shader calculates positions of fullscreen triangle based on index, so no need to bind vertex/index buffers to create a fullscreen quad
		vkCmdDraw(visBuffShadeCommandBuffers[i], 3, 1, 0, 0);

		// Now end the render pass
		vkCmdEndRenderPass(visBuffShadeCommandBuffers[i]);

		// And end recording of command buffers
		if (vkEndCommandBuffer(visBuffShadeCommandBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record vis Buff Shade command buffer");
		}
	}
}

void VulkanApplication::RecordVisBuffWriteCommandBuffer()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(vulkan->Device(), &allocInfo, &visBuffWriteCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vis buff write command buffer");
	}

	// Create the semaphore to synchronise the visibility buffer write pass
	VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	if (vkCreateSemaphore(vulkan->Device(), &semaphoreInfo, nullptr, &visBuffWriteSemaphore) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vis buff write pass semaphore");
	}

	// Set up clear values for each attachment
	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	// Begin recording the command buffer
	VkCommandBufferBeginInfo cmdBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	if (vkBeginCommandBuffer(visBuffWriteCommandBuffer, &cmdBeginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to begin recording vis buff write command buffer");
	}

	// Render Pass info
	VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassInfo.renderPass = visBuffWriteRenderPass;
	renderPassInfo.framebuffer = visibilityBuffer.frameBuffer;
	renderPassInfo.renderArea.extent = vulkan->Swapchain().Extent();
	renderPassInfo.clearValueCount = SCAST_U32(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	// Begin the render pass
	vkCmdBeginRenderPass(visBuffWriteCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {};
	viewport.width = (float)vulkan->Swapchain().Extent().width;
	viewport.height = (float)vulkan->Swapchain().Extent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(visBuffWriteCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent = vulkan->Swapchain().Extent();
	scissor.offset = { 0, 0 };
	vkCmdSetScissor(visBuffWriteCommandBuffer, 0, 1, &scissor);

	// Bind descriptor set
	vkCmdBindDescriptorSets(visBuffWriteCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffWritePipelineLayout, 0, 1, &writePassDescriptorSet, 0, nullptr);

	// Bind the pipeline
	vkCmdBindPipeline(visBuffWriteCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffWritePipeline);

	// Bind geometry buffers
	VkDeviceSize offsets[1] = { 0 };
	VkBuffer vertexBuffers[] = { vertexBuffer.VkHandle() };
	vkCmdBindVertexBuffers(visBuffWriteCommandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(visBuffWriteCommandBuffer, indexBuffer.VkHandle(), 0, VK_INDEX_TYPE_UINT32);

	// Draw using index buffer
	vkCmdDrawIndexed(visBuffWriteCommandBuffer, SCAST_U32(terrain.Indices().size()), 1, 0, 0, 0);

	vkCmdEndRenderPass(visBuffWriteCommandBuffer);

	// End recording of command buffer
	if (vkEndCommandBuffer(visBuffWriteCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to record vis buff write command buffer");
	}
}

VkCommandBuffer VulkanApplication::BeginSingleTimeCommands()
{
	// Set up a command buffer to perform the data transfer
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(vulkan->Device(), &allocInfo, &commandBuffer);

	// Begin recording
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanApplication::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	// Now execute the command buffer
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(vulkan->PhysDevice().Queues()->graphics, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vulkan->PhysDevice().Queues()->graphics); // Maybe use a fence here if performing multiple transfers, allowing driver to optimise

	vkFreeCommandBuffers(vulkan->Device(), commandPool, 1, &commandBuffer);
}
#pragma endregion

#pragma region Depth Buffer Functions
void VulkanApplication::CreateDepthResources()
{
	// Select format
	VkFormat depthFormat = FindDepthFormat();

	// Create Image and ImageView objects
	visibilityBuffer.depth.Create(vulkan->Swapchain().Extent().width, vulkan->Swapchain().Extent().height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);
	visibilityBuffer.depth.CreateImageView(vulkan->Device(), VK_IMAGE_ASPECT_DEPTH_BIT);

	// Transition depth image for shader usage
	visibilityBuffer.depth.TransitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, vulkan->Device(), vulkan->PhysDevice(), commandPool);

	// Now set up the depth attachments for the shade pass (TODO: may be completely unnecessary)
	// Create Image and ImageView objects
	visBuffShadeDepthImage.Create(vulkan->Swapchain().Extent().width, vulkan->Swapchain().Extent().height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);
	visBuffShadeDepthImage.CreateImageView(vulkan->Device(), VK_IMAGE_ASPECT_DEPTH_BIT);

	// Transition depth image for shader usage
	visBuffShadeDepthImage.TransitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, vulkan->Device(), vulkan->PhysDevice(), commandPool);
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
void VulkanApplication::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage allocUsage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VmaAllocation& allocation)
{
	// Specify buffer info
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// Specify memory allocation Info
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = allocUsage;
	allocInfo.requiredFlags = properties;

	// Create buffer
	if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer");
	}
}

void VulkanApplication::CreateVertexBuffer()
{
	// Create staging buffer on host memory
	VkDeviceSize bufferSize = sizeof(terrain.Vertices()[0]) * terrain.Vertices().size();
	Buffer stagingBuffer;
	stagingBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

	// Map vertex data to staging buffer memory allocation
	stagingBuffer.MapData(terrain.Vertices().data(), allocator);

	// Create vertex buffer on device local memory
	vertexBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

	// Copy data to new vertex buffer
	CopyBuffer(stagingBuffer.VkHandle(), vertexBuffer.VkHandle(), bufferSize);

	stagingBuffer.CleanUp(allocator);
}

void VulkanApplication::CreateAttributeBuffer()
{
	// Create staging buffer on host memory
	VkDeviceSize bufferSize = sizeof(terrain.PackedVertexAttributes()[0]) * terrain.PackedVertexAttributes().size();
	Buffer stagingBuffer;
	stagingBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

	// Map attribute data to staging buffer memory allocation
	stagingBuffer.MapData(terrain.PackedVertexAttributes().data(), allocator);

	// Create attribute buffer on device local memory
	vertexAttributeBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

	// Copy data to new attribute buffer
	CopyBuffer(stagingBuffer.VkHandle(), vertexAttributeBuffer.VkHandle(), bufferSize);

	stagingBuffer.CleanUp(allocator);
}

void VulkanApplication::CreateIndexBuffer()
{
	// Create staging buffer on host memory
	VkDeviceSize bufferSize = sizeof(terrain.Indices()[0]) * terrain.Indices().size();
	Buffer stagingBuffer;
	stagingBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

	// Map index data to staging buffer memory allocation
	stagingBuffer.MapData(terrain.Indices().data(), allocator);

	// Create index buffer on device local memory
	indexBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

	// Copy data to new index buffer
	CopyBuffer(stagingBuffer.VkHandle(), indexBuffer.VkHandle(), bufferSize);

	stagingBuffer.CleanUp(allocator);
}

void VulkanApplication::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	// Create uniform buffer for shade Pass
	mvpUniformBuffer.Create(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);
}

void VulkanApplication::UpdateMVPUniformBuffer()
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

	// Fill Uniform Buffer
	UniformBufferObject ubo = {};
	ubo.mvp = (projMatrix * viewMatrix) * modelMatrix;
	ubo.proj = projMatrix;
	//ubo.invViewProj = inverseViewProj;

	// Now map the memory to uniform buffer
	mvpUniformBuffer.MapData(&ubo, allocator);
}

void VulkanApplication::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// Copy the data
	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	EndSingleTimeCommands(commandBuffer);
}

void VulkanApplication::CreateVmaAllocator()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = vulkan->PhysDevice().VkHandle();
	allocatorInfo.device = vulkan->Device();
	vmaCreateAllocator(&allocatorInfo, &allocator);
}
#pragma endregion

#pragma region Texture Functions
void VulkanApplication::CreateDepthSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR; // Apply linear interpolation to over/under-sampled texels
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_TRUE; // Enable anisotropic filtering 
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; // Clamp texel coordinates to [0, 1]
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 11.0f;

	if (vkCreateSampler(vulkan->Device(), &samplerInfo, nullptr, &depthSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create depth sampler");
	}
}

#pragma region Descriptor Functions
void VulkanApplication::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = SCAST_U32((vulkan->Swapchain().Images().size())) + 1; // Extra mvp ubo for the write pass
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = SCAST_U32(vulkan->Swapchain().Images().size()) * 2; // 2 sampled images per swapchain image
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[2].descriptorCount = (SCAST_U32(vulkan->Swapchain().Images().size())) * 2; // 2 storage buffers per swapchain image

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = SCAST_U32(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = SCAST_U32(vulkan->Swapchain().Images().size()) + 1; // 1 descriptor set per swapchain image and one for the write pass

	if (vkCreateDescriptorPool(vulkan->Device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool");
	}
}

void VulkanApplication::CreateShadePassDescriptorSetLayout()
{
	// Descriptor layout for the shading pass
	// Binding 0: Model texture sampler
	VkDescriptorSetLayoutBinding textureSamplerBinding = {};
	textureSamplerBinding.binding = 0;
	textureSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureSamplerBinding.descriptorCount = 1;
	textureSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 1: Visibility Buffer
	VkDescriptorSetLayoutBinding visBufferBinding = {};
	visBufferBinding.binding = 1;
	visBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	visBufferBinding.descriptorCount = 1;
	visBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Binding 2: MVP Uniform Buffer
	VkDescriptorSetLayoutBinding modelUboLayoutBinding = {};
	modelUboLayoutBinding.binding = 2;
	modelUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	modelUboLayoutBinding.descriptorCount = 1;
	modelUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Specify that this descriptor will be used in the fragment shader

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

	// Create descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 5> bindings = { modelUboLayoutBinding, textureSamplerBinding, visBufferBinding, indexBufferBinding, attributeBufferBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = SCAST_U32(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(vulkan->Device(), &layoutInfo, nullptr, &shadePassDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shade pass descriptor set layout");
	}
}

void VulkanApplication::CreateWritePassDescriptorSetLayout()
{
	// Descriptor layout for the write pass
	// Binding 0: Vertex Shader Uniform Buffer of loaded model
	VkDescriptorSetLayoutBinding modelUboLayoutBinding = {};
	modelUboLayoutBinding.binding = 0;
	modelUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	modelUboLayoutBinding.descriptorCount = 1;
	modelUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Specify that this descriptor will be used in the vertex shader

	// Create descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 1> bindings = { modelUboLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = SCAST_U32(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(vulkan->Device(), &layoutInfo, nullptr, &writePassDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create write pass descriptor set layout");
	}
}

// Create the descriptor sets for the shade pass, containing the visibility buffer images (for each swapchain image)
void VulkanApplication::CreateShadePassDescriptorSets()
{
	// Create shade descriptor sets
	std::vector<VkDescriptorSetLayout> shadingLayouts(vulkan->Swapchain().Images().size(), shadePassDescriptorSetLayout);

	// Create shade pass descriptor set
	VkDescriptorSetAllocateInfo shadePassAllocInfo = {};
	shadePassAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	shadePassAllocInfo.descriptorPool = descriptorPool;
	shadePassAllocInfo.descriptorSetCount = SCAST_U32(vulkan->Swapchain().Images().size());
	shadePassAllocInfo.pSetLayouts = shadingLayouts.data();

	shadePassDescriptorSets.resize(vulkan->Swapchain().Images().size());
	if (vkAllocateDescriptorSets(vulkan->Device(), &shadePassAllocInfo, shadePassDescriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate shade pass descriptor sets");
	}

	// Configure the descriptors
	for (size_t i = 0; i < vulkan->Swapchain().Images().size(); i++)
	{
		// Model diffuse texture
		VkDescriptorImageInfo modelTextureInfo = {};
		modelTextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		modelTextureInfo.imageView = terrainTexture.GetImage().ImageView();
		modelTextureInfo.sampler = terrainTexture.Sampler();

		// Visibility Buffer
		VkDescriptorImageInfo visBufferInfo = {};
		visBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		visBufferInfo.imageView = visibilityBuffer.visibility.ImageView();
		visBufferInfo.sampler = terrainTexture.Sampler();

		// Model UBO
		mvpUniformBuffer.SetupDescriptor(sizeof(UniformBufferObject), 0);

		// Index Buffer
		indexBuffer.SetupDescriptor(sizeof(terrain.Indices()[0]) * terrain.Indices().size(), 0);

		// Vertex Attribute Buffer
		vertexAttributeBuffer.SetupDescriptor(sizeof(terrain.PackedVertexAttributes()[0]) * terrain.PackedVertexAttributes().size(), 0);

		// Create a descriptor write for each descriptor in the set
		std::array<VkWriteDescriptorSet, 5> shadePassDescriptorWrites = {};

		// Binding 0: Model texture sampler
		shadePassDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[0].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[0].dstBinding = 0;
		shadePassDescriptorWrites[0].dstArrayElement = 0;
		shadePassDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadePassDescriptorWrites[0].descriptorCount = 1;
		shadePassDescriptorWrites[0].pImageInfo = &modelTextureInfo;

		// Binding 1: Visibility Buffer
		shadePassDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[1].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[1].dstBinding = 1;
		shadePassDescriptorWrites[1].dstArrayElement = 0;
		shadePassDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadePassDescriptorWrites[1].descriptorCount = 1;
		shadePassDescriptorWrites[1].pImageInfo = &visBufferInfo;

		// Binding 2: Vertex Shader Uniform Buffer of loaded model
		shadePassDescriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[2].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[2].dstBinding = 2;
		shadePassDescriptorWrites[2].dstArrayElement = 0;
		shadePassDescriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadePassDescriptorWrites[2].descriptorCount = 1;
		shadePassDescriptorWrites[2].pBufferInfo = mvpUniformBuffer.DescriptorInfo();

		// Binding 3: Index Buffer
		shadePassDescriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[3].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[3].dstBinding = 3;
		shadePassDescriptorWrites[3].dstArrayElement = 0;
		shadePassDescriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadePassDescriptorWrites[3].descriptorCount = 1;
		shadePassDescriptorWrites[3].pBufferInfo = indexBuffer.DescriptorInfo();

		// Binding 4: Vertex Attribute Buffer
		shadePassDescriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[4].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[4].dstBinding = 4;
		shadePassDescriptorWrites[4].dstArrayElement = 0;
		shadePassDescriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadePassDescriptorWrites[4].descriptorCount = 1;
		shadePassDescriptorWrites[4].pBufferInfo = vertexAttributeBuffer.DescriptorInfo();

		vkUpdateDescriptorSets(vulkan->Device(), SCAST_U32(shadePassDescriptorWrites.size()), shadePassDescriptorWrites.data(), 0, nullptr);
	}
}

// Create the descriptor sets for the write pass, containing the MVP uniform buffer only
void VulkanApplication::CreateWritePassDescriptorSet()
{
	// Create write pass descriptor sets
	std::vector<VkDescriptorSetLayout> writeLayouts = { writePassDescriptorSetLayout };

	// Create write pass descriptor set for MVP UBO
	VkDescriptorSetAllocateInfo writePassAllocInfo = {};
	writePassAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	writePassAllocInfo.descriptorPool = descriptorPool;
	writePassAllocInfo.descriptorSetCount = 1;
	writePassAllocInfo.pSetLayouts = writeLayouts.data();

	if (vkAllocateDescriptorSets(vulkan->Device(), &writePassAllocInfo, &writePassDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate write pass descriptor sets");
	}

	// Model UBO
	mvpUniformBuffer.SetupDescriptor(sizeof(UniformBufferObject), 0);

	// Create a descriptor write for each descriptor in the set
	std::array<VkWriteDescriptorSet, 1> writePassDescriptorWrites = {};

	// Binding 0: Vertex Shader Uniform Buffer of loaded model
	writePassDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writePassDescriptorWrites[0].dstSet = writePassDescriptorSet;
	writePassDescriptorWrites[0].dstBinding = 0;
	writePassDescriptorWrites[0].dstArrayElement = 0;
	writePassDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writePassDescriptorWrites[0].descriptorCount = 1;
	writePassDescriptorWrites[0].pBufferInfo = mvpUniformBuffer.DescriptorInfo();

	vkUpdateDescriptorSets(vulkan->Device(), SCAST_U32(writePassDescriptorWrites.size()), writePassDescriptorWrites.data(), 0, nullptr);
}
#pragma endregion

#pragma region Other Functions
std::vector<char> VulkanApplication::ReadFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file: " + filename);
	}

	// Get size of the file and create a buffer
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	// Read all bytes
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

void VulkanApplication::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}


#pragma endregion 

} // namespace