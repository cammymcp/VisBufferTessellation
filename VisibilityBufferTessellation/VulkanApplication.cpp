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
	CreateGeometryRenderPass();
	CreateDeferredRenderPass();
	CreateGeometryDescriptorSetLayout();
	CreateDeferredDescriptorSetLayout();
	CreatePipelineCache();
	CreateGeometryPipeline();
	CreateDeferredPipeline();
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
	CreateGBufferSampler();
	LoadModel();
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateFullScreenQuad();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateFrameBuffers();
	CreateDescriptorSets();
	AllocateDeferredCommandBuffers();
	AllocateGeometryCommandBuffer();
	CreateSynchronisationObjects();
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
	vkDestroySampler(vulkan->Device(), gBufferSampler, nullptr);
	vkDestroyImageView(vulkan->Device(), gBuffer.position.imageView, nullptr);
	vmaDestroyImage(allocator, gBuffer.position.image, gBuffer.position.imageMemory);
	vkDestroyImageView(vulkan->Device(), gBuffer.normal.imageView, nullptr);
	vmaDestroyImage(allocator, gBuffer.normal.image, gBuffer.normal.imageMemory);
	vkDestroyImageView(vulkan->Device(), gBuffer.colour.imageView, nullptr);
	vmaDestroyImage(allocator, gBuffer.colour.image, gBuffer.colour.imageMemory);
	vkDestroyImageView(vulkan->Device(), textureImageView, nullptr);
	vmaDestroyImage(allocator, textureImage, textureImageMemory);

	// Destroy Descriptor Pool
	vkDestroyDescriptorPool(vulkan->Device(), descriptorPool, nullptr);

	// Destroy descriptor layouts
	vkDestroyDescriptorSetLayout(vulkan->Device(), deferredDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkan->Device(), geometryDescriptorSetLayout, nullptr);

	// Destroy uniform buffers
	for (size_t i = 0; i < vulkan->SwapChainImages().size(); i++)
	{
		vmaDestroyBuffer(allocator, uniformBuffers[i], uniformBufferAllocations[i]);
	}
	vmaDestroyBuffer(allocator, geometryUniformBuffer, geometryUniformBufferAllocation);

	// Destroy vertex and index buffers
	vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);
	vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);
	vmaDestroyBuffer(allocator, fsQuadVertexBuffer, fsQuadVertexMemory);
	vmaDestroyBuffer(allocator, fsQuadIndexBuffer, fsQuadIndexMemory);
	vmaDestroyAllocator(allocator);

	// Destroy Sync Objects
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(vulkan->Device(), renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(vulkan->Device(), imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(vulkan->Device(), inFlightFences[i], nullptr);
	}
	vkDestroySemaphore(vulkan->Device(), geometryPassSemaphore, nullptr);

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
	CreateGeometryRenderPass();
	CreateDeferredRenderPass();
	CreatePipelineCache();
	CreateGeometryPipeline();
	CreateDeferredPipeline();
	CreateDepthResources();
	CreateFrameBuffers();
	AllocateDeferredCommandBuffers();
	AllocateGeometryCommandBuffer();
}

void VulkanApplication::CleanUpSwapChain()
{
	// Destroy depth buffers
	vkDestroyImageView(vulkan->Device(), gBuffer.depth.imageView, nullptr);
	vmaDestroyImage(allocator, gBuffer.depth.image, gBuffer.depth.imageMemory);
	vkDestroyImageView(vulkan->Device(), deferredDepthImageView, nullptr);
	vmaDestroyImage(allocator, deferredDepthImage, deferredDepthImageMemory);

	// Destroy frame buffers
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
	{
		vkDestroyFramebuffer(vulkan->Device(), swapChainFramebuffers[i], nullptr);
	}
	vkDestroyFramebuffer(vulkan->Device(), gBuffer.frameBuffer, nullptr);

	// Free command buffers
	vkFreeCommandBuffers(vulkan->Device(), commandPool, static_cast<uint32_t>(deferredCommandBuffers.size()), deferredCommandBuffers.data());
	vkFreeCommandBuffers(vulkan->Device(), commandPool, 1, &geometryCommandBuffer);
	vkDestroyPipeline(vulkan->Device(), deferredPipeline, nullptr);
	vkDestroyPipeline(vulkan->Device(), geometryPipeline, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), deferredPipelineLayout, nullptr);
	vkDestroyPipelineLayout(vulkan->Device(), geometryPipelineLayout, nullptr);
	vkDestroyPipelineCache(vulkan->Device(), pipelineCache, nullptr);
	vkDestroyRenderPass(vulkan->Device(), deferredRenderPass, nullptr);
	vkDestroyRenderPass(vulkan->Device(), gBuffer.renderPass, nullptr);
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

void VulkanApplication::CreateGeometryPipeline()
{
	// Create geometry shader stages from compiled shader code
	auto vertShaderCode = ReadFile("shaders/geometry.vert.spv");
	auto fragShaderCode = ReadFile("shaders/geometry.frag.spv");

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
	VkPipelineShaderStageCreateInfo geometryShaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

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

	// We need to set up color blend attachments for all of the gbuffer color attachments in the subpass (position, normal, albedo)
	VkPipelineColorBlendAttachmentState posBlendAttachment = {};
	posBlendAttachment.colorWriteMask = 0xf;
	posBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendAttachmentState normBlendAttachment = {};
	normBlendAttachment.colorWriteMask = 0xf;
	normBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendAttachmentState colBlendAttachment = {};
	colBlendAttachment.colorWriteMask = 0xf;
	colBlendAttachment.blendEnable = VK_FALSE;
	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachments = { posBlendAttachment, normBlendAttachment, colBlendAttachment };
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
	CreateGeometryPipelineLayout();

	// We now have everything we need to create the geometry graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.layout = geometryPipelineLayout;
	pipelineInfo.renderPass = gBuffer.renderPass;
	pipelineInfo.subpass = 0; // Index of the sub pass where this pipeline will be used
	pipelineInfo.pStages = geometryShaderStages;
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

	// Now create the geometry pass pipeline
	if (vkCreateGraphicsPipelines(vulkan->Device(), pipelineCache, 1, &pipelineInfo, nullptr, &geometryPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create geometry pipeline");
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
	pipelineLayoutInfo.pSetLayouts = &deferredDescriptorSetLayout;
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

void VulkanApplication::CreateGeometryPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &geometryDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;

	// Geometry layout
	if (vkCreatePipelineLayout(vulkan->Device(), &pipelineLayoutInfo, nullptr, &geometryPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create geometry pipeline layout");
	}
}

void VulkanApplication::CreateGeometryRenderPass()
{
	// Create gBuffer attachments
	CreateFrameBufferAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &gBuffer.position);
	CreateFrameBufferAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &gBuffer.normal);
	CreateFrameBufferAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &gBuffer.colour);
	CreateDepthResources();

	// Create attachment descriptions for the gbuffer
	std::array<VkAttachmentDescription, 4> attachmentDescs = {};

	// Fill attachment properties
	for (uint32_t i = 0; i < 4; ++i)
	{
		attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 3) // Depth attachment
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}
	// Fill Formats
	attachmentDescs[0].format = gBuffer.position.format;
	attachmentDescs[1].format = gBuffer.normal.format;
	attachmentDescs[2].format = gBuffer.colour.format;
	attachmentDescs[3].format = gBuffer.depth.format;

	// Create attachment references for the subpass to use
	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	VkAttachmentReference depthReference = {3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

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
	if (vkCreateRenderPass(vulkan->Device(), &renderPassInfo, nullptr, &gBuffer.renderPass) != VK_SUCCESS)
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
	// Create gBuffer
	std::array<VkImageView, 4> attachments = {};
	attachments[0] = gBuffer.position.imageView;
	attachments[1] = gBuffer.normal.imageView;
	attachments[2] = gBuffer.colour.imageView;
	attachments[3] = gBuffer.depth.imageView;

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.pNext = NULL;
	framebufferInfo.renderPass = gBuffer.renderPass; // Tell frame buffer which render pass it should be compatible with
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = vulkan->SwapChainExtent().width;
	framebufferInfo.height = vulkan->SwapChainExtent().height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(vulkan->Device(), &framebufferInfo, nullptr, &gBuffer.frameBuffer) != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to create frame buffer");
	}

	// Create deferred frame buffer for each swapchain image
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
	vkWaitForFences(vulkan->Device(), 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	// Acquire image from swap chain. ImageAvailableSemaphore will be signaled when the image is ready to be drawn to. Check if we have to recreate the swap chain
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(vulkan->Device(), vulkan->SwapChain(), std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
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
	vkResetFences(vulkan->Device(), 1, &inFlightFences[currentFrame]);

	// Update the uniform buffer
	UpdateDeferredUniformBuffer(imageIndex);
	UpdateGeometryUniformBuffer();

	/// GEOMETRY PASS =======================================================================================
	// Submit the command buffer. Waits for the provided semaphores to be signaled before beginning execution
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // Waiting until we can start writing color, in theory this means the implementation can execute the vertex buffer while image isn't available
	submitInfo.pWaitDstStageMask = waitStages;

	// Wait for image to be available, and signal when g-buffer is filled
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &geometryPassSemaphore;

	// Submit to queue
	submitInfo.pCommandBuffers = &geometryCommandBuffer;
	submitInfo.commandBufferCount = 1;
	if (vkQueueSubmit(vulkan->Queues().graphics, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit draw command buffer");
	}
	/// =====================================================================================================

	/// DEFERRED PASS =======================================================================================
	// Wait for g-buffer being filled, signal when rendering is complete
	submitInfo.pWaitSemaphores = &geometryPassSemaphore;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

	// Submit to queue
	submitInfo.pCommandBuffers = &deferredCommandBuffers[imageIndex];
	if (vkQueueSubmit(vulkan->Queues().graphics, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit draw command buffer");
	}
	/// =====================================================================================================

	// Now submit the resulting image back to the swap chain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame]; // Wait for deferred pass to finish

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

// We'll need to set up semaphores to ensure the order of the asynchronous functions
void VulkanApplication::CreateSynchronisationObjects()
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Signal the fence so that the first frame is rendered

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(vulkan->Device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(vulkan->Device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(vulkan->Device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{

			throw std::runtime_error("Failed to create syncrhonisation objects for a frame");
		}
	}
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
		vkCmdBindDescriptorSets(deferredCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipelineLayout, 0, 1, &deferredDescriptorSets[i], 0, nullptr);

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

void VulkanApplication::AllocateGeometryCommandBuffer()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(vulkan->Device(), &allocInfo, &geometryCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate geometry command buffer");
	}

	// Create the semaphore to synchronise the geometry pass
	VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	if (vkCreateSemaphore(vulkan->Device(), &semaphoreInfo, nullptr, &geometryPassSemaphore) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create geometry pass semaphore");
	}

	// Set up clear values for each attachment
	std::array<VkClearValue, 4> clearValues = {};
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[3].depthStencil = { 1.0f, 0 };

	// Begin recording the command buffer
	VkCommandBufferBeginInfo cmdBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	if (vkBeginCommandBuffer(geometryCommandBuffer, &cmdBeginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to begin recording geometry command buffer");
	}

	// Render Pass info
	VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassInfo.renderPass = gBuffer.renderPass;
	renderPassInfo.framebuffer = gBuffer.frameBuffer;
	renderPassInfo.renderArea.extent = vulkan->SwapChainExtent();
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	// Begin the render pass
	vkCmdBeginRenderPass(geometryCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {};
	viewport.width = (float)vulkan->SwapChainExtent().width;
	viewport.height = (float)vulkan->SwapChainExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(geometryCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent = vulkan->SwapChainExtent();
	scissor.offset = { 0, 0 };
	vkCmdSetScissor(geometryCommandBuffer, 0, 1, &scissor);

	// Bind the pipeline
	vkCmdBindPipeline(geometryCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPipeline);

	// Bind descriptor sets
	vkCmdBindDescriptorSets(geometryCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPipelineLayout, 0, 1, &geometryDescriptorSet, 0, nullptr);

	// Bind geometry buffers
	VkDeviceSize offsets[1] = { 0 };
	VkBuffer vertexBuffers[] = { vertexBuffer };
	vkCmdBindVertexBuffers(geometryCommandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(geometryCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// Draw using index buffer
	vkCmdDrawIndexed(geometryCommandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	vkCmdEndRenderPass(geometryCommandBuffer);

	// End recording of command buffer
	if (vkEndCommandBuffer(geometryCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to record geometry command buffer");
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
	gBuffer.depth.format = depthFormat;

	// Create Image and ImageView objects
	CreateImage(vulkan->SwapChainExtent().width, vulkan->SwapChainExtent().height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBuffer.depth.image, gBuffer.depth.imageMemory);
	gBuffer.depth.imageView = VulkanCore::CreateImageView(vulkan->Device(), gBuffer.depth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Transition depth image for shader usage
	TransitionImageLayout(gBuffer.depth.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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
	uniformBuffers.resize(vulkan->SwapChainImages().size());
	uniformBufferAllocations.resize(vulkan->SwapChainImages().size());
	for (size_t i = 0; i < vulkan->SwapChainImages().size(); i++)
	{
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBufferAllocations[i]);
	}

	// Create uniform buffer for Geometry Pass
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, geometryUniformBuffer, geometryUniformBufferAllocation);
}

void VulkanApplication::UpdateDeferredUniformBuffer(uint32_t currentImage)
{
	// Generate matrices
	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1.0f);
	ubo.proj = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
	//ubo.proj[1][1] *= -1; // Flip Y of projection matrix to account for OpenGL's flipped Y clip axis

	// Now map the memory to uniform buffer
	void* mappedData;
	vmaMapMemory(allocator, uniformBufferAllocations[currentImage], &mappedData);
	memcpy(mappedData, &ubo, sizeof(ubo));
	vmaUnmapMemory(allocator, uniformBufferAllocations[currentImage]);
}

void VulkanApplication::UpdateGeometryUniformBuffer()
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
	vmaMapMemory(allocator, geometryUniformBufferAllocation, &mappedData);
	memcpy(mappedData, &ubo, sizeof(ubo));
	vmaUnmapMemory(allocator, geometryUniformBufferAllocation);
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

void VulkanApplication::CreateGBufferSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	if (vkCreateSampler(vulkan->Device(), &samplerInfo, nullptr, &gBufferSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler");
	}
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
void VulkanApplication::CreateDeferredDescriptorSetLayout()
{
	// Descriptor layout for the deferred pass
	// Binding 0: Vertex Shader Uniform Buffer
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Specify that this descriptor will be used in the vertex shader

	// Binding 1: Position render target sampler
	VkDescriptorSetLayoutBinding positionSamplerBinding = {};
	positionSamplerBinding.binding = 1;
	positionSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	positionSamplerBinding.descriptorCount = 1;
	positionSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Binding 2: Normals render target sampler
	VkDescriptorSetLayoutBinding normalsSamplerBinding = {};
	normalsSamplerBinding.binding = 2;
	normalsSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalsSamplerBinding.descriptorCount = 1;
	normalsSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Binding 3: Colour render target sampler
	VkDescriptorSetLayoutBinding colourSamplerBinding = {};
	colourSamplerBinding.binding = 3;
	colourSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colourSamplerBinding.descriptorCount = 1;
	colourSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Create descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 4> bindings = { uboLayoutBinding, positionSamplerBinding, normalsSamplerBinding, colourSamplerBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(vulkan->Device(), &layoutInfo, nullptr, &deferredDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create deferred descriptor set layout");
	}
}

void VulkanApplication::CreateGeometryDescriptorSetLayout()
{
	// Descriptor layout for the geometry pass
	// Binding 0: Vertex Shader Uniform Buffer of loaded model
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Specify that this descriptor will be used in the vertex shader

	// Binding 1: Model texture sampler
	VkDescriptorSetLayoutBinding textureSamplerBinding = {};
	textureSamplerBinding.binding = 1;
	textureSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureSamplerBinding.descriptorCount = 1;
	textureSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Used in the fragment shader

	// Create descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, textureSamplerBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(vulkan->Device(), &layoutInfo, nullptr, &geometryDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create geometry descriptor set layout");
	}
}

void VulkanApplication::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(vulkan->SwapChainImages().size()) + 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = (3 * static_cast<uint32_t>(vulkan->SwapChainImages().size())) + 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(vulkan->SwapChainImages().size()) + 1;

	if (vkCreateDescriptorPool(vulkan->Device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool");
	}
}

// Create two descriptor sets, one containing the gBuffer images for the deferred pass (for each swapchain image), and one with the MVP uniform buffer and texture for the loaded model
void VulkanApplication::CreateDescriptorSets()
{
	// Create deferred descriptor sets
	std::vector<VkDescriptorSetLayout> deferredLayouts(vulkan->SwapChainImages().size(), deferredDescriptorSetLayout);

	// Create deferred pass descriptor set for textured quad
	VkDescriptorSetAllocateInfo deferredAllocInfo = {};
	deferredAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	deferredAllocInfo.descriptorPool = descriptorPool;
	deferredAllocInfo.descriptorSetCount = static_cast<uint32_t>(vulkan->SwapChainImages().size());
	deferredAllocInfo.pSetLayouts = deferredLayouts.data();

	deferredDescriptorSets.resize(vulkan->SwapChainImages().size());
	if (vkAllocateDescriptorSets(vulkan->Device(), &deferredAllocInfo, deferredDescriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate deferred descriptor sets");
	}

	// Configure the descriptors
	for (size_t i = 0; i < vulkan->SwapChainImages().size(); i++)
	{
		VkDescriptorBufferInfo deferredBufferInfo = {};
		deferredBufferInfo.buffer = uniformBuffers[i];
		deferredBufferInfo.offset = 0;
		deferredBufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo texDescriptorPosition = {};
		texDescriptorPosition.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texDescriptorPosition.imageView = gBuffer.position.imageView;
		texDescriptorPosition.sampler = gBufferSampler;

		VkDescriptorImageInfo texDescriptorNormal = {};
		texDescriptorNormal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texDescriptorNormal.imageView = gBuffer.normal.imageView;
		texDescriptorNormal.sampler = gBufferSampler;

		VkDescriptorImageInfo texDescriptorColour = {};
		texDescriptorColour.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texDescriptorColour.imageView = gBuffer.colour.imageView;
		texDescriptorColour.sampler = gBufferSampler;

		// Create a descriptor write for each descriptor in the set
		std::array<VkWriteDescriptorSet, 4> deferredDescriptorWrites = {};

		// Binding 0: Vertex Shader Uniform Buffer
		deferredDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		deferredDescriptorWrites[0].dstSet = deferredDescriptorSets[i];
		deferredDescriptorWrites[0].dstBinding = 0;
		deferredDescriptorWrites[0].dstArrayElement = 0;
		deferredDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		deferredDescriptorWrites[0].descriptorCount = 1;
		deferredDescriptorWrites[0].pBufferInfo = &deferredBufferInfo;

		// Binding 1: Position render texture
		deferredDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		deferredDescriptorWrites[1].dstSet = deferredDescriptorSets[i];
		deferredDescriptorWrites[1].dstBinding = 1;
		deferredDescriptorWrites[1].dstArrayElement = 0;
		deferredDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		deferredDescriptorWrites[1].descriptorCount = 1;
		deferredDescriptorWrites[1].pImageInfo = &texDescriptorPosition;

		// Binding 2: Normals render texture
		deferredDescriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		deferredDescriptorWrites[2].dstSet = deferredDescriptorSets[i];
		deferredDescriptorWrites[2].dstBinding = 2;
		deferredDescriptorWrites[2].dstArrayElement = 0;
		deferredDescriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		deferredDescriptorWrites[2].descriptorCount = 1;
		deferredDescriptorWrites[2].pImageInfo = &texDescriptorNormal;

		// Binding 3: Colour render texture
		deferredDescriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		deferredDescriptorWrites[3].dstSet = deferredDescriptorSets[i];
		deferredDescriptorWrites[3].dstBinding = 3;
		deferredDescriptorWrites[3].dstArrayElement = 0;
		deferredDescriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		deferredDescriptorWrites[3].descriptorCount = 1;
		deferredDescriptorWrites[3].pImageInfo = &texDescriptorColour;

		vkUpdateDescriptorSets(vulkan->Device(), static_cast<uint32_t>(deferredDescriptorWrites.size()), deferredDescriptorWrites.data(), 0, nullptr);
	}

	// Create Geometry Pass Descriptor Sets
	std::vector<VkDescriptorSetLayout> geometryLayouts = { geometryDescriptorSetLayout };
	VkDescriptorSetAllocateInfo geometryAllocInfo = {};
	geometryAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	geometryAllocInfo.descriptorPool = descriptorPool;
	geometryAllocInfo.descriptorSetCount = static_cast<uint32_t>(geometryLayouts.size());
	geometryAllocInfo.pSetLayouts = geometryLayouts.data();

	if (vkAllocateDescriptorSets(vulkan->Device(), &geometryAllocInfo, &geometryDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate geometry descriptor sets");
	}

	VkDescriptorBufferInfo geometryBufferInfo = {};
	geometryBufferInfo.buffer = geometryUniformBuffer;
	geometryBufferInfo.offset = 0;
	geometryBufferInfo.range = sizeof(UniformBufferObject);

	VkDescriptorImageInfo modelTextureInfo = {};
	modelTextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	modelTextureInfo.imageView = textureImageView;
	modelTextureInfo.sampler = textureSampler;

	// Create a descriptor write for each descriptor in the set
	std::array<VkWriteDescriptorSet, 2> geometryDescriptorWrites = {};

	// Binding 0: Vertex Shader Uniform Buffer
	geometryDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	geometryDescriptorWrites[0].dstSet = geometryDescriptorSet;
	geometryDescriptorWrites[0].dstBinding = 0;
	geometryDescriptorWrites[0].dstArrayElement = 0;
	geometryDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	geometryDescriptorWrites[0].descriptorCount = 1;
	geometryDescriptorWrites[0].pBufferInfo = &geometryBufferInfo;

	// Binding 1: Model texture
	geometryDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	geometryDescriptorWrites[1].dstSet = geometryDescriptorSet;
	geometryDescriptorWrites[1].dstBinding = 1;
	geometryDescriptorWrites[1].dstArrayElement = 0;
	geometryDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	geometryDescriptorWrites[1].descriptorCount = 1;
	geometryDescriptorWrites[1].pImageInfo = &modelTextureInfo;

	vkUpdateDescriptorSets(vulkan->Device(), static_cast<uint32_t>(geometryDescriptorWrites.size()), geometryDescriptorWrites.data(), 0, nullptr);
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