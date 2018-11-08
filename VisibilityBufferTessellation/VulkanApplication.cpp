#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "VulkanApplication.h"

namespace vbt { 

#pragma region Core Functions
void VulkanApplication::InitWindow()
{
	// Init glfw and do not create an OpenGL context
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// Create window
	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, FrameBufferResizeCallback);
}

void VulkanApplication::Init()
{
	// Initialise core objects and functionality
	vulkan = new VulkanCore();
	vulkan->Init(window);
	CreateVmaAllocator();
	CreateCommandPool();
	CreateVisBuffWriteRenderPass();
	CreateDeferredRenderPass();
	CreateShadePassDescriptorSetLayout();
	CreateWritePassDescriptorSetLayout();
	CreatePipelineCache();
	CreateVisBuffWritePipeline();
	CreateDeferredPipeline();
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
	CreateDepthSampler();
	LoadModel();
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateFullScreenQuad();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateFrameBuffers();
	CreateShadePassDescriptorSets();
	CreateWritePassDescriptorSet();
	AllocateDeferredCommandBuffers();
	AllocateVisBuffWriteCommandBuffer();
}

void VulkanApplication::Update()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		DrawFrame();
	}

	// Wait for the device to finish up any operations when exiting the main loop
	vkDeviceWaitIdle(vulkan->Device());
}

void VulkanApplication::CleanUp()
{
	CleanUpSwapChain();

	// Destroy texture objects
	vkDestroySampler(vulkan->Device(), textureSampler, nullptr);
	vkDestroySampler(vulkan->Device(), depthSampler, nullptr);
	vkDestroyImageView(vulkan->Device(), visibilityBuffer.visAndBarys.imageView, nullptr);
	vmaDestroyImage(allocator, visibilityBuffer.visAndBarys.image, visibilityBuffer.visAndBarys.imageMemory);
	vkDestroyImageView(vulkan->Device(), visibilityBuffer.uvDerivs.imageView, nullptr);
	vmaDestroyImage(allocator, visibilityBuffer.uvDerivs.image, visibilityBuffer.uvDerivs.imageMemory);
	vkDestroyImageView(vulkan->Device(), textureImageView, nullptr);
	vmaDestroyImage(allocator, textureImage, textureImageMemory);

	// Destroy Descriptor Pool
	vkDestroyDescriptorPool(vulkan->Device(), descriptorPool, nullptr);

	// Destroy descriptor layouts
	vkDestroyDescriptorSetLayout(vulkan->Device(), shadePassDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkan->Device(), writePassDescriptorSetLayout, nullptr);

	// Destroy uniform buffers
	for (size_t i = 0; i < vulkan->SwapChainImages().size(); i++)
	{
		vmaDestroyBuffer(allocator, quadUniformBuffers[i], quadUniformBufferAllocations[i]);
	}
	vmaDestroyBuffer(allocator, mvpUniformBuffer, mvpUniformBufferAllocation);

	// Destroy vertex and index buffers
	vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);
	vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);
	vmaDestroyBuffer(allocator, fsQuadVertexBuffer, fsQuadVertexMemory);
	vmaDestroyBuffer(allocator, fsQuadIndexBuffer, fsQuadIndexMemory);
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
	vulkan->RecreateSwapChain(window);
	CreateVisBuffWriteRenderPass();
	CreateDeferredRenderPass();
	CreatePipelineCache();
	CreateVisBuffWritePipeline();
	CreateDeferredPipeline();
	CreateDepthResources();
	CreateFrameBuffers();
	AllocateDeferredCommandBuffers();
	AllocateVisBuffWriteCommandBuffer();
}

void VulkanApplication::CleanUpSwapChain()
{
	// Destroy depth buffers
	vkDestroyImageView(vulkan->Device(), visibilityBuffer.depth.imageView, nullptr);
	vmaDestroyImage(allocator, visibilityBuffer.depth.image, visibilityBuffer.depth.imageMemory);
	vkDestroyImageView(vulkan->Device(), deferredDepthImageView, nullptr);
	vmaDestroyImage(allocator, deferredDepthImage, deferredDepthImageMemory);

	// Destroy frame buffers
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
	{
		vkDestroyFramebuffer(vulkan->Device(), swapChainFramebuffers[i], nullptr);
	}
	vkDestroyFramebuffer(vulkan->Device(), visibilityBuffer.frameBuffer, nullptr);

	// Free command buffers
	vkFreeCommandBuffers(vulkan->Device(), commandPool, static_cast<uint32_t>(deferredCommandBuffers.size()), deferredCommandBuffers.data());
	vkFreeCommandBuffers(vulkan->Device(), commandPool, 1, &visBuffWriteCommandBuffer);
	vkDestroyPipeline(vulkan->Device(), deferredPipeline, nullptr);
	vkDestroyPipeline(vulkan->Device(), visBuffWritePipeline, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), deferredPipelineLayout, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), visBuffWritePipelineLayout, nullptr);
	vkDestroyPipelineCache(vulkan->Device(), pipelineCache, nullptr);
	vkDestroyRenderPass(vulkan->Device(), deferredRenderPass, nullptr);
	vkDestroyRenderPass(vulkan->Device(), visBuffWriteRenderPass, nullptr);
}
#pragma endregion

#pragma region Graphics Pipeline Functions
// Creation of the graphics pipeline requires four objects:
// Shader stages: the shader modules that define the functionality of the programmable stages of the pipeline
// Fixed-function state: all of the structures that define the fixed-function stages of the pipeline
// Pipeline Layout: the uniform and push values referenced by the shader that can be updated at draw time
// Render pass: the attachments referenced by the pipeline stages and their usage
void VulkanApplication::CreateDeferredPipeline()
{
	// Create deferred shader stages from compiled shader code
	auto vertShaderCode = ReadFile("shaders/deferred.vert.spv");
	auto fragShaderCode = ReadFile("shaders/deferred.frag.spv");

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
	VkPipelineShaderStageCreateInfo deferredShaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };	

	// Set up topology input format
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Set up viewport to be the whole of the swap chain images
	//VkViewport viewport = {};
	//viewport.x = 0.0f;
	//viewport.y = 0.0f;
	//viewport.width = (float)vulkan->SwapChainExtent().width;
	//viewport.height = (float)vulkan->SwapChainExtent().height;
	//viewport.minDepth = 0.0f;
	//viewport.maxDepth = 1.0f;
	//
	//// Set the scissor to the whole of the framebuffer, eg no cropping 
	//VkRect2D scissor = {};
	//scissor.offset = { 0, 0 };
	//scissor.extent = vulkan->SwapChainExtent();

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
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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
	VkPipelineColorBlendStateCreateInfo colourBlending = {};
	colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlending.logicOpEnable = VK_FALSE;
	colourBlending.attachmentCount = 1;
	colourBlending.pAttachments = &colourBlendAttachment;

	// Dynamic State
	std::vector<VkDynamicState> dynamicStateEnables = {	VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR	};
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicState.flags = 0;

	// PipelineLayout
	CreateDeferredPipelineLayout();

	// We now have everything we need to create the deferred graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = deferredShaderStages;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colourBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = deferredPipelineLayout;
	pipelineInfo.renderPass = deferredRenderPass;
	pipelineInfo.subpass = 0; // Index of the sub pass where this pipeline will be used
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; 
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.flags = 0;
	// Empty vertex input state, quads are generated by the vertex shader
	VkPipelineVertexInputStateCreateInfo emptyInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	pipelineInfo.pVertexInputState = &emptyInputState;
	if (vkCreateGraphicsPipelines(vulkan->Device(), pipelineCache, 1, &pipelineInfo, nullptr, &deferredPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create deferred pipeline");
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
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// Set up topology input format
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Set up viewport to be the whole of the swap chain images
	//VkViewport viewport = {};
	//viewport.x = 0.0f;
	//viewport.y = 0.0f;
	//viewport.width = (float)vulkan->SwapChainExtent().width;
	//viewport.height = (float)vulkan->SwapChainExtent().height;
	//viewport.minDepth = 0.0f;
	//viewport.maxDepth = 1.0f;
	//
	//// Set the scissor to the whole of the framebuffer, eg no cropping 
	//VkRect2D scissor = {};
	//scissor.offset = { 0, 0 };
	//scissor.extent = vulkan->SwapChainExtent();

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

	// We need to set up color blend attachments for all of the visibility buffer color attachments in the subpass (vis and barys, uv derivs)
	VkPipelineColorBlendAttachmentState visBlendAttachment = {};
	visBlendAttachment.colorWriteMask = 0xf;
	visBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendAttachmentState derivsBlendAttachment = {};
	derivsBlendAttachment.colorWriteMask = 0xf;
	derivsBlendAttachment.blendEnable = VK_FALSE;
	std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments = { visBlendAttachment, derivsBlendAttachment };
	VkPipelineColorBlendStateCreateInfo colourBlending = {};
	colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlending.logicOpEnable = VK_FALSE;
	colourBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
	colourBlending.pAttachments = blendAttachments.data();

	// Dynamic State
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
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

void VulkanApplication::CreatePipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (vkCreatePipelineCache(vulkan->Device(), &pipelineCacheCreateInfo, nullptr, &pipelineCache) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline cache");
	}
}

void VulkanApplication::CreateDeferredPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &shadePassDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;

	// Deferred Layout
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &deferredPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create deferred pipeline layout");
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
	// Create gBuffer attachments
	CreateFrameBufferAttachment(VK_FORMAT_R32G32_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &visibilityBuffer.visAndBarys);
	CreateFrameBufferAttachment(VK_FORMAT_R32G32_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &visibilityBuffer.uvDerivs);
	CreateDepthResources();

	// Create attachment descriptions for the visibility buffer
	std::array<VkAttachmentDescription, 3> attachmentDescs = {};

	// Fill attachment properties
	for (uint32_t i = 0; i < 3; ++i)
	{
		attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 2) // Depth attachment
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // TODO: The visibility buffer is set to the PRESENT layout in other implementations. I think thats a mistake but want to put it in here in case it doesn't work
		}
	}
	// Fill Formats
	attachmentDescs[0].format = visibilityBuffer.visAndBarys.format;
	attachmentDescs[1].format = visibilityBuffer.uvDerivs.format;
	attachmentDescs[2].format = visibilityBuffer.depth.format;

	// Create attachment references for the subpass to use
	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	VkAttachmentReference depthReference = {2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	// Create subpass (one for now)
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Specify that this is a graphics subpass, not compute
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());;
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
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.pAttachments = attachmentDescs.data();
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

void VulkanApplication::CreateDeferredRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = vulkan->SwapChainImageFormat();
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = FindDepthFormat();
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
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
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	// Create deferred render pass
	if (vkCreateRenderPass(vulkan->Device(), &renderPassInfo, nullptr, &deferredRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create deferred render pass");
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
void VulkanApplication::CreateFrameBuffers()
{
	// Create Visibility Buffer frame buffer
	std::array<VkImageView, 3> attachments = {};
	attachments[0] = visibilityBuffer.visAndBarys.imageView;
	attachments[1] = visibilityBuffer.uvDerivs.imageView;
	attachments[2] = visibilityBuffer.depth.imageView;

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.pNext = NULL;
	framebufferInfo.renderPass = visBuffWriteRenderPass; // Tell frame buffer which render pass it should be compatible with
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = vulkan->SwapChainExtent().width;
	framebufferInfo.height = vulkan->SwapChainExtent().height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(vulkan->Device(), &framebufferInfo, nullptr, &visibilityBuffer.frameBuffer) != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to create frame buffer");
	}

	// Create visibility buffer shade frame buffer for each swapchain image
	swapChainFramebuffers.resize(vulkan->SwapChainImageViews().size());
	for (size_t i = 0; i < vulkan->SwapChainImageViews().size(); i++)
	{
		std::array<VkImageView, 2> attachments =
		{
			vulkan->SwapChainImageViews()[i],
			deferredDepthImageView
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.pNext = NULL;
		framebufferInfo.renderPass = deferredRenderPass; // Tell frame buffer which render pass it should be compatible with
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = vulkan->SwapChainExtent().width;
		framebufferInfo.height = vulkan->SwapChainExtent().height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vulkan->Device(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to create frame buffer");
		}
	}
}

void VulkanApplication::CreateFrameBufferAttachment(VkFormat format, VkImageUsageFlags usage, FrameBufferAttachment* attachment)
{
	// Set format
	attachment->format = format;

	// Create image
	CreateImage(vulkan->SwapChainExtent().width, vulkan->SwapChainExtent().height, format, VK_IMAGE_TILING_OPTIMAL, usage, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachment->image, attachment->imageMemory);

	// Create image view
	VkImageAspectFlags aspectMask = 0;
	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	attachment->imageView = VulkanCore::CreateImageView(vulkan->Device(), attachment->image, format, aspectMask);
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
	VkResult result = vkAcquireNextImageKHR(vulkan->Device(), vulkan->SwapChain(), std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
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
	UpdateQuadUniformBuffer(imageIndex); // TODO: Don't need to update this every frame, its static. It's probably pointless even having it as a uniform buffer
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
	if (vkQueueSubmit(vulkan->Queues().graphics, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
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
	submitInfo.pCommandBuffers = &deferredCommandBuffers[imageIndex];
	if (vkQueueSubmit(vulkan->Queues().graphics, 1, &submitInfo, vulkan->Fences()[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit visBuffShade command buffer");
	}
	/// =====================================================================================================

	// Now submit the resulting image back to the swap chain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // Wait for deferred pass to finish

	VkSwapchainKHR swapChains[] = { vulkan->SwapChain() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	// Request to present image to the swap chain
	result = vkQueuePresentKHR(vulkan->Queues().present, &presentInfo);
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
	QueueFamilyIndices queueFamilyIndices = vulkan->FindQueueFamilies(vulkan->PhysicalDevice());

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	poolInfo.flags = 0; // Optional. Allows command buffers to be reworked at runtime

	if (vkCreateCommandPool(vulkan->Device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

void VulkanApplication::AllocateDeferredCommandBuffers()
{
	deferredCommandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(deferredCommandBuffers.size());

	if (vkAllocateCommandBuffers(vulkan->Device(), &allocInfo, deferredCommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate deferred command buffers");
	}

	// Define clear values
	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = CLEAR_COLOUR;
	clearValues[1].depthStencil = { 1.0f, 0 };

	// Begin recording command buffers
	for (size_t i = 0; i < deferredCommandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(deferredCommandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording deferred command buffer");
		}

		// Start the render pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = deferredRenderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = vulkan->SwapChainExtent();
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());;
		renderPassInfo.pClearValues = clearValues.data();

		// The first parameter for every command is always the command buffer to record the command to.
		vkCmdBeginRenderPass(deferredCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = {};
		viewport.width = (float)vulkan->SwapChainExtent().width;
		viewport.height = (float)vulkan->SwapChainExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(deferredCommandBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.extent = vulkan->SwapChainExtent();
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(deferredCommandBuffers[i], 0, 1, &scissor);

		// Bind the correct descriptor set for this swapchain image
		vkCmdBindDescriptorSets(deferredCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipelineLayout, 0, 1, &shadePassDescriptorSets[i], 0, nullptr);

		// Now bind the graphics pipeline
		vkCmdBindPipeline(deferredCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipeline);

		// Bind the vertex and index buffers
		VkBuffer quadVertexBuffers[] = { fsQuadVertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(deferredCommandBuffers[i], 0, 1, quadVertexBuffers, offsets);
		vkCmdBindIndexBuffer(deferredCommandBuffers[i], fsQuadIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

		// Draw data using the index buffer
		vkCmdDrawIndexed(deferredCommandBuffers[i], 6, 1, 0, 0, 0);

		// Now end the render pass
		vkCmdEndRenderPass(deferredCommandBuffers[i]);

		// And end recording of command buffers
		if (vkEndCommandBuffer(deferredCommandBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record deferred command buffer");
		}
	}
}

void VulkanApplication::AllocateVisBuffWriteCommandBuffer()
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
	std::array<VkClearValue, 3> clearValues = {};
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].depthStencil = { 1.0f, 0 };

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
	renderPassInfo.renderArea.extent = vulkan->SwapChainExtent();
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	// Begin the render pass
	vkCmdBeginRenderPass(visBuffWriteCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {};
	viewport.width = (float)vulkan->SwapChainExtent().width;
	viewport.height = (float)vulkan->SwapChainExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(visBuffWriteCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent = vulkan->SwapChainExtent();
	scissor.offset = { 0, 0 };
	vkCmdSetScissor(visBuffWriteCommandBuffer, 0, 1, &scissor);

	// Bind descriptor set
	vkCmdBindDescriptorSets(visBuffWriteCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipelineLayout, 0, 1, &writePassDescriptorSet, 0, nullptr);

	// Bind the pipeline
	vkCmdBindPipeline(visBuffWriteCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, visBuffWritePipeline);

	// Bind geometry buffers
	VkDeviceSize offsets[1] = { 0 };
	VkBuffer vertexBuffers[] = { vertexBuffer };
	vkCmdBindVertexBuffers(visBuffWriteCommandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(visBuffWriteCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// Draw using index buffer
	vkCmdDrawIndexed(visBuffWriteCommandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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

	vkQueueSubmit(vulkan->Queues().graphics, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vulkan->Queues().graphics); // Maybe use a fence here if performing multiple transfers, allowing driver to optimise

	vkFreeCommandBuffers(vulkan->Device(), commandPool, 1, &commandBuffer);
}
#pragma endregion

#pragma region Deferred Rendering Functions
// Creates the quad that the deferred rendering pass will display the final result to.
void VulkanApplication::CreateFullScreenQuad()
{
	struct QuadVertex
	{
		glm::vec3 pos;
		glm::vec3 colour;
		glm::vec2 tex;
	};

	// Set up vertices (counter clockwise)
	std::vector<QuadVertex> quadVertexBuffer;
	quadVertexBuffer.push_back({ {-1.0f, 1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } }); // Bottom left
	quadVertexBuffer.push_back({ { 1.0f, 1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } }); // Bottom right
	quadVertexBuffer.push_back({ { 1.0f,-1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } }); // Top Right
	quadVertexBuffer.push_back({ {-1.0f,-1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } }); // Top Left

	// Create staging buffer on host memory
	VkBuffer vertexStagingBuffer;
	VmaAllocation vertexStagingBufferAllocation;
	VkDeviceSize vertexBufferSize = quadVertexBuffer.size() * sizeof(QuadVertex);
	CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingBufferAllocation);

	// Map vertex data to staging buffer memory allocation
	void* mappedVertexData;
	vmaMapMemory(allocator, vertexStagingBufferAllocation, &mappedVertexData);
	memcpy(mappedVertexData, quadVertexBuffer.data(), (size_t)vertexBufferSize);
	vmaUnmapMemory(allocator, vertexStagingBufferAllocation);

	// Create vertex buffer on device local memory
	CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fsQuadVertexBuffer, fsQuadVertexMemory);

	// Copy data to new vertex buffer
	CopyBuffer(vertexStagingBuffer, fsQuadVertexBuffer, vertexBufferSize);

	// Set up indices
	VkBuffer indexStagingBuffer;
	VmaAllocation indexStagingBufferAllocation;
	std::vector<uint16_t> quadIndexBuffer = { 1, 0, 2,  2, 3, 0 };
	VkDeviceSize indexBufferSize = sizeof(uint16_t) * quadIndexBuffer.size();
	CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufferAllocation);

	// Map vertex data to staging buffer memory allocation
	void* mappedIndexData;
	vmaMapMemory(allocator, indexStagingBufferAllocation, &mappedIndexData);
	memcpy(mappedIndexData, quadIndexBuffer.data(), (size_t)indexBufferSize);
	vmaUnmapMemory(allocator, indexStagingBufferAllocation);

	// Create index buffer on device local memory
	CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fsQuadIndexBuffer, fsQuadIndexMemory);

	// Copy data to new index buffer
	CopyBuffer(indexStagingBuffer, fsQuadIndexBuffer, indexBufferSize);

	// Clean up staging buffers
	vmaDestroyBuffer(allocator, vertexStagingBuffer, vertexStagingBufferAllocation);
	vmaDestroyBuffer(allocator, indexStagingBuffer, indexStagingBufferAllocation);
}
#pragma endregion

#pragma region Depth Buffer Functions
void VulkanApplication::CreateDepthResources()
{
	// Select format
	VkFormat depthFormat = FindDepthFormat();
	visibilityBuffer.depth.format = depthFormat;

	// Create Image and ImageView objects
	CreateImage(vulkan->SwapChainExtent().width, vulkan->SwapChainExtent().height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, visibilityBuffer.depth.image, visibilityBuffer.depth.imageMemory);
	visibilityBuffer.depth.imageView = VulkanCore::CreateImageView(vulkan->Device(), visibilityBuffer.depth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Transition depth image for shader usage
	TransitionImageLayout(visibilityBuffer.depth.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	// Now set up the depth attachments for the deferred pass (TODO: may be completely unnecessary)
	// Create Image and ImageView objects
	CreateImage(vulkan->SwapChainExtent().width, vulkan->SwapChainExtent().height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, deferredDepthImage, deferredDepthImageMemory);
	deferredDepthImageView = VulkanCore::CreateImageView(vulkan->Device(), deferredDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Transition depth image for shader usage
	TransitionImageLayout(deferredDepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
		vkGetPhysicalDeviceFormatProperties(vulkan->PhysicalDevice(), format, &props);

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

bool HasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
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
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// Create staging buffer on host memory
	VkBuffer stagingBuffer;
	VmaAllocation stagingBufferAllocation;
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferAllocation);

	// Map vertex data to staging buffer memory allocation
	void* mappedData;
	vmaMapMemory(allocator, stagingBufferAllocation, &mappedData);
	memcpy(mappedData, vertices.data(), (size_t)bufferSize);
	vmaUnmapMemory(allocator, stagingBufferAllocation);

	// Create vertex buffer on device local memory
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferAllocation);

	// Copy data to new vertex buffer
	CopyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	// Clean up staging buffer
	vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
}

void VulkanApplication::CreateIndexBuffer()
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	// Create staging buffer on host memory
	VkBuffer stagingBuffer;
	VmaAllocation stagingBufferAllocation;
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferAllocation);

	// Map vertex data to staging buffer memory allocation
	void* mappedData;
	vmaMapMemory(allocator, stagingBufferAllocation, &mappedData);
	memcpy(mappedData, indices.data(), (size_t)bufferSize);
	vmaUnmapMemory(allocator, stagingBufferAllocation);

	// Create vertex buffer on device local memory
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferAllocation);

	// Copy data to new vertex buffer
	CopyBuffer(stagingBuffer, indexBuffer, bufferSize);

	// Clean up staging buffer
	vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
}

void VulkanApplication::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	// Create deferred uniform buffers for each swap chain image
	quadUniformBuffers.resize(vulkan->SwapChainImages().size());
	quadUniformBufferAllocations.resize(vulkan->SwapChainImages().size());
	for (size_t i = 0; i < vulkan->SwapChainImages().size(); i++)
	{
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, quadUniformBuffers[i], quadUniformBufferAllocations[i]);
	}

	// Create uniform buffer for Geometry Pass
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mvpUniformBuffer, mvpUniformBufferAllocation);
}

void VulkanApplication::UpdateQuadUniformBuffer(uint32_t currentImage)
{
	// Generate matrices
	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1.0f);
	ubo.proj = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
	//ubo.proj[1][1] *= -1; // Flip Y of projection matrix to account for OpenGL's flipped Y clip axis

	// Now map the memory to uniform buffer
	void* mappedData;
	vmaMapMemory(allocator, quadUniformBufferAllocations[currentImage], &mappedData);
	memcpy(mappedData, &ubo, sizeof(ubo));
	vmaUnmapMemory(allocator, quadUniformBufferAllocations[currentImage]);
}

void VulkanApplication::UpdateMVPUniformBuffer()
{
	// Get time since rendering started
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	// Now generate the model, view and projection matrices
	UniformBufferObject ubo = {};
	//glm::mat4 modelMat = glm::scale(glm::mat4(1.0f), glm::vec3(1.f, 1.f, 1.f));
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), vulkan->SwapChainExtent().width / (float)vulkan->SwapChainExtent().height, 0.1f, 10.0f);
	ubo.proj[1][1] *= -1; // Flip Y of projection matrix to account for OpenGL's flipped Y clip axis

	// Now map the memory to uniform buffer
	void* mappedData;
	vmaMapMemory(allocator, mvpUniformBufferAllocation, &mappedData);
	memcpy(mappedData, &ubo, sizeof(ubo));
	vmaUnmapMemory(allocator, mvpUniformBufferAllocation);
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
	allocatorInfo.physicalDevice = vulkan->PhysicalDevice();
	allocatorInfo.device = vulkan->Device();
	vmaCreateAllocator(&allocatorInfo, &allocator);
}
#pragma endregion

#pragma region Texture Functions
void VulkanApplication::CreateTextureImage()
{
	// Load image file with STB library
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	if (!pixels)
	{
		throw std::runtime_error("Failed to load texture image");
	}

	// Create a staging buffer for the pixel data
	VkBuffer stagingBuffer;
	VmaAllocation stagingBufferMemory;
	CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// Copy the pixel values directly to the buffer
	void* mappedData;
	vmaMapMemory(allocator, stagingBufferMemory, &mappedData);
	memcpy(mappedData, pixels, static_cast<uint32_t>(imageSize));
	vmaUnmapMemory(allocator, stagingBufferMemory);

	// Clean up pixel memory
	stbi_image_free(pixels);

	// Now create Vulkan Image object
	CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

	// Transition the texture image to the correct layout for recieving the pixel data from the buffer
	TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Execute the copy from buffer to image
	CopyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	// Now transition the image layout again to allow for sampling in the shader
	TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Free up the staging buffer
	vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferMemory);
}

void VulkanApplication::CreateTextureImageView()
{
	textureImageView = VulkanCore::CreateImageView(vulkan->Device(), textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanApplication::CreateTextureSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR; // Apply linear interpolation to over/under-sampled texels
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE; // Enable anisotropic filtering 
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; // Clamp texel coordinates to [0, 1]
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(vulkan->Device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler");
	}
}

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

void VulkanApplication::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& allocation)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // Optional

	//Create allocation info
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = memoryUsage;
	allocInfo.requiredFlags = properties;

	if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image");
	}
}

void VulkanApplication::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout srcLayout, VkImageLayout dstLayout)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// Use an image memory barrier to perform the layout transition
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = srcLayout;
	barrier.newLayout = dstLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Don't bother transitioning queue family
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	// Determine aspect mask
	if (dstLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (HasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	// Since barriers are primarily used for synchronisation, we must specify which processes to wait on, and which processes should wait on this
	// We need to handle the transition from Undefined to Transfer Destination, from Transfer Destination to Shader Access and from Undefinded to Depth Attachment
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (srcLayout == VK_IMAGE_LAYOUT_UNDEFINED && dstLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (srcLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && dstLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (srcLayout == VK_IMAGE_LAYOUT_UNDEFINED && dstLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::invalid_argument("Unsupported layout transition");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	EndSingleTimeCommands(commandBuffer);
}

void VulkanApplication::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	EndSingleTimeCommands(commandBuffer);
}
#pragma endregion

#pragma region Model Functions
void VulkanApplication::LoadModel()
{
	tinyobj::attrib_t attribute;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	// LoadObj automatically triangulates faces by default, so we don't need to worry about that 
	if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &err, MODEL_PATH.c_str()))
	{
		throw std::runtime_error(err);
	}

	// Use a map to track the index of unique vertices
	std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

	// Combine all shapes into one model
	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex = {};

			// Vertices array is an array of floats, so we have to multiply the index by three each time, and offset by 0/1/2 to get the X/Y/Z components
			vertex.pos =
			{
				attribute.vertices[3 * index.vertex_index + 0],
				attribute.vertices[3 * index.vertex_index + 1],
				attribute.vertices[3 * index.vertex_index + 2]
			};
			vertex.texCoord =
			{
				attribute.texcoords[2 * index.texcoord_index + 0],
				attribute.texcoords[2 * index.texcoord_index + 1] 
			};
			vertex.colour = { 1.0f, 1.0f, 1.0f };
			vertices.push_back(vertex);

			// Check if we've already seen this vertex before, and add it to the map if not
			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}
			indices.push_back(uniqueVertices[vertex]);
		}
	}
}
#pragma endregion

#pragma region Descriptor Functions
void VulkanApplication::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkan->SwapChainImages().size()) + 1; // Extra mvp ubo for the write pass
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(vulkan->SwapChainImages().size());
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	poolSizes[2].descriptorCount = (static_cast<uint32_t>(vulkan->SwapChainImages().size())) * 3; // 3 input attachments per swapchain image

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(vulkan->SwapChainImages().size()) + 1; // 1 descriptor set per swapchain image and one for the write pass

	if (vkCreateDescriptorPool(vulkan->Device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool");
	}
}

void VulkanApplication::CreateShadePassDescriptorSetLayout()
{
	// Descriptor layout for the shading pass
	// Binding 0: Vertex Shader Uniform Buffer of loaded model
	VkDescriptorSetLayoutBinding modelUboLayoutBinding = {};
	modelUboLayoutBinding.binding = 0;
	modelUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	modelUboLayoutBinding.descriptorCount = 1;
	modelUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Specify that this descriptor will be used in the vertex shader

	// Binding 1: Fullscreen Quad Uniform Buffer
	VkDescriptorSetLayoutBinding quadUboLayoutBinding = {};
	quadUboLayoutBinding.binding = 1;
	quadUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	quadUboLayoutBinding.descriptorCount = 1;
	quadUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Specify that this descriptor will be used in the vertex shader

	// Binding 2: Model texture sampler
	VkDescriptorSetLayoutBinding textureSamplerBinding = {};
	textureSamplerBinding.binding = 2;
	textureSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureSamplerBinding.descriptorCount = 1;
	textureSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Binding 3: Visibility and Barycentric Coords Buffer
	VkDescriptorSetLayoutBinding visBufferBinding = {};
	visBufferBinding.binding = 3;
	visBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	visBufferBinding.descriptorCount = 1;
	visBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Binding 4: UV Derivatives Buffer
	VkDescriptorSetLayoutBinding uvDerivBufferBinding = {};
	uvDerivBufferBinding.binding = 4;
	uvDerivBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	uvDerivBufferBinding.descriptorCount = 1;
	uvDerivBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Binding 5: Depth Buffer
	VkDescriptorSetLayoutBinding depthBufferBinding = {};
	depthBufferBinding.binding = 5;
	depthBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthBufferBinding.descriptorCount = 1;
	depthBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Create descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 6> bindings = { modelUboLayoutBinding, quadUboLayoutBinding, textureSamplerBinding, visBufferBinding, uvDerivBufferBinding, depthBufferBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
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
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
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
	std::vector<VkDescriptorSetLayout> shadingLayouts(vulkan->SwapChainImages().size(), shadePassDescriptorSetLayout);

	// Create shade pass descriptor set
	VkDescriptorSetAllocateInfo shadePassAllocInfo = {};
	shadePassAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	shadePassAllocInfo.descriptorPool = descriptorPool;
	shadePassAllocInfo.descriptorSetCount = static_cast<uint32_t>(vulkan->SwapChainImages().size());
	shadePassAllocInfo.pSetLayouts = shadingLayouts.data();

	shadePassDescriptorSets.resize(vulkan->SwapChainImages().size());
	if (vkAllocateDescriptorSets(vulkan->Device(), &shadePassAllocInfo, shadePassDescriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate shade pass descriptor sets");
	}

	// Configure the descriptors
	for (size_t i = 0; i < vulkan->SwapChainImages().size(); i++)
	{
		// Model UBO
		VkDescriptorBufferInfo mvpUBOInfo = {};
		mvpUBOInfo.buffer = mvpUniformBuffer;
		mvpUBOInfo.offset = 0;
		mvpUBOInfo.range = sizeof(UniformBufferObject);

		// Quad UBO
		VkDescriptorBufferInfo quadUBOInfo = {};
		quadUBOInfo.buffer = quadUniformBuffers[i];
		quadUBOInfo.offset = 0;
		quadUBOInfo.range = sizeof(UniformBufferObject);

		// Model diffuse texture
		VkDescriptorImageInfo modelTextureInfo = {};
		modelTextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		modelTextureInfo.imageView = textureImageView;
		modelTextureInfo.sampler = textureSampler;

		// Visibility and Barycentric Coords Buffer
		VkDescriptorImageInfo visBufferInfo = {};
		visBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		visBufferInfo.imageView = visibilityBuffer.visAndBarys.imageView;
		visBufferInfo.sampler = VK_NULL_HANDLE;

		// UV Derivatives Buffer
		VkDescriptorImageInfo uvDerivBufferInfo = {};
		uvDerivBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		uvDerivBufferInfo.imageView = visibilityBuffer.uvDerivs.imageView;
		uvDerivBufferInfo.sampler = VK_NULL_HANDLE;

		// Depth Buffer
		VkDescriptorImageInfo depthBufferInfo = {};
		depthBufferInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthBufferInfo.imageView = visibilityBuffer.depth.imageView;
		depthBufferInfo.sampler = depthSampler;

		// Create a descriptor write for each descriptor in the set
		std::array<VkWriteDescriptorSet, 6> shadePassDescriptorWrites = {};

		// Binding 0: Vertex Shader Uniform Buffer of loaded model
		shadePassDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[0].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[0].dstBinding = 0;
		shadePassDescriptorWrites[0].dstArrayElement = 0;
		shadePassDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadePassDescriptorWrites[0].descriptorCount = 1;
		shadePassDescriptorWrites[0].pBufferInfo = &mvpUBOInfo;

		// Binding 1: Fullscreen Quad Uniform Buffer
		shadePassDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[1].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[1].dstBinding = 1;
		shadePassDescriptorWrites[1].dstArrayElement = 0;
		shadePassDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadePassDescriptorWrites[1].descriptorCount = 1;
		shadePassDescriptorWrites[1].pBufferInfo = &quadUBOInfo;

		// Binding 2: Model texture sampler
		shadePassDescriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[2].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[2].dstBinding = 2;
		shadePassDescriptorWrites[2].dstArrayElement = 0;
		shadePassDescriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadePassDescriptorWrites[2].descriptorCount = 1;
		shadePassDescriptorWrites[2].pImageInfo = &modelTextureInfo;

		// Binding 3: Visibility and Barycentric Coords Buffer
		shadePassDescriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[3].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[3].dstBinding = 3;
		shadePassDescriptorWrites[3].dstArrayElement = 0;
		shadePassDescriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		shadePassDescriptorWrites[3].descriptorCount = 1;
		shadePassDescriptorWrites[3].pImageInfo = &visBufferInfo;

		// Binding 4: UV Derivatives Buffer
		shadePassDescriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[4].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[4].dstBinding = 4;
		shadePassDescriptorWrites[4].dstArrayElement = 0;
		shadePassDescriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		shadePassDescriptorWrites[4].descriptorCount = 1;
		shadePassDescriptorWrites[4].pImageInfo = &uvDerivBufferInfo;

		// Binding 5: Depth Buffer
		shadePassDescriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadePassDescriptorWrites[5].dstSet = shadePassDescriptorSets[i];
		shadePassDescriptorWrites[5].dstBinding = 5;
		shadePassDescriptorWrites[5].dstArrayElement = 0;
		shadePassDescriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		shadePassDescriptorWrites[5].descriptorCount = 1;
		shadePassDescriptorWrites[5].pImageInfo = &depthBufferInfo;

		vkUpdateDescriptorSets(vulkan->Device(), static_cast<uint32_t>(shadePassDescriptorWrites.size()), shadePassDescriptorWrites.data(), 0, nullptr);
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
	VkDescriptorBufferInfo mvpUBOInfo = {};
	mvpUBOInfo.buffer = mvpUniformBuffer;
	mvpUBOInfo.offset = 0;
	mvpUBOInfo.range = sizeof(UniformBufferObject);

	// Create a descriptor write for each descriptor in the set
	std::array<VkWriteDescriptorSet, 1> writePassDescriptorWrites = {};

	// Binding 0: Vertex Shader Uniform Buffer of loaded model
	writePassDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writePassDescriptorWrites[0].dstSet = writePassDescriptorSet;
	writePassDescriptorWrites[0].dstBinding = 0;
	writePassDescriptorWrites[0].dstArrayElement = 0;
	writePassDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writePassDescriptorWrites[0].descriptorCount = 1;
	writePassDescriptorWrites[0].pBufferInfo = &mvpUBOInfo;

	vkUpdateDescriptorSets(vulkan->Device(), static_cast<uint32_t>(writePassDescriptorWrites.size()), writePassDescriptorWrites.data(), 0, nullptr);
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