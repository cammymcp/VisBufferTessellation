#include "VbtImGUI.h"
#include "VulkanApplication.h"

namespace vbt
{
	ImGUI::ImGUI(VulkanApplication* vulkanApp, VmaAllocator* allocator) : vulkanAppHandle(vulkanApp), allocatorHandle(allocator)
	{
		deviceHandle = vulkanApp->GetVulkanCore()->DevicePtr();
		ImGui::CreateContext();
	}

	ImGUI::~ImGUI() {}
	
	void ImGUI::Init(float width, float height)
	{
		// Colour style
		ImGui::StyleColorsDark();
		// Dimensions
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(width, height);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	}

	void ImGUI::CreateVulkanResources(PhysicalDevice physDevice, VkCommandPool& cmdPool, VkRenderPass renderPass, VkQueue copyQueue)
	{
		ImGuiIO& io = ImGui::GetIO();

		// Create font image
		unsigned char* fontData;
		int texWidth, texHeight;
		io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
		VkDeviceSize fontBufferSize = texWidth * texHeight * 4 * sizeof(char);

		// Create a staging buffer for the font data
		Buffer stagingBuffer;
		stagingBuffer.Create(fontBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, *allocatorHandle);

		// Copy the pixel values directly to the buffer
		stagingBuffer.MapData(fontData, *allocatorHandle);

		// Create font image in GPU memory
		fontImage.Create(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *allocatorHandle);

		// Transition the texture image to the correct layout for recieving font data from the buffer
		fontImage.TransitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *deviceHandle, physDevice, cmdPool);

		// Execute the copy from buffer to image
		fontImage.CopyFromBuffer(stagingBuffer.VkHandle(), *deviceHandle, physDevice, cmdPool);

		// Now transition the image layout again to allow for sampling in the shader
		fontImage.TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *deviceHandle, physDevice, cmdPool);

		// Free up the staging buffer
		stagingBuffer.CleanUp(*allocatorHandle);

		// Now create the image view and the sampler
		fontImage.CreateImageView(*deviceHandle, VK_IMAGE_ASPECT_COLOR_BIT);
		fontImage.CreateSampler(*deviceHandle, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

		// Descriptor Pool
		std::array<VkDescriptorPoolSize, 1> poolSizes = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[0].descriptorCount = 1;
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = SCAST_U32(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = SCAST_U32(vulkanAppHandle->GetVulkanCore()->Swapchain().Images().size());
		if (vkCreateDescriptorPool(*deviceHandle, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui descriptor pool");
		}

		// Descriptor Set Layout
		// Binding 0: Font texture sampler
		VkDescriptorSetLayoutBinding samplerBinding = {};
		samplerBinding.binding = 0;
		samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerBinding.descriptorCount = 1;
		samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		std::array<VkDescriptorSetLayoutBinding, 1> bindings = { samplerBinding };
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = SCAST_U32(bindings.size());
		layoutInfo.pBindings = bindings.data();
		if (vkCreateDescriptorSetLayout(*deviceHandle, &layoutInfo, nullptr, &descriptorLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui descriptor set layout");
		}

		// Descriptor Set
		std::vector<VkDescriptorSetLayout> layouts = { descriptorLayout };
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(*deviceHandle, &allocInfo, &descriptorSet) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate ImGui descriptor sets");
		}
		// Font Sampler Descriptor
		fontImage.SetUpDescriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		fontImage.SetupDescriptorWriteSet(descriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
		std::array<VkWriteDescriptorSet, 1> descriptorWrites = { fontImage.WriteDescriptorSet() };
		vkUpdateDescriptorSets(*deviceHandle, SCAST_U32(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

		// Pipeline Cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		if (vkCreatePipelineCache(*deviceHandle, &pipelineCacheCreateInfo, nullptr, &pipelineCache) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui pipeline cache");
		}

		// Push Constant for rendering parameters
		VkPushConstantRange pushConstantRange = {};
		pushConstantRange.size = sizeof(RenderParamsPushConstant);
		pushConstantRange.offset = 0;
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		std::vector<VkPushConstantRange> pushConstantRanges = { pushConstantRange };

		// Pipeline Layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorLayout;
		pipelineLayoutInfo.pushConstantRangeCount = SCAST_U32(pushConstantRanges.size());
		pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
		pipelineLayoutInfo.pNext = nullptr;
		pipelineLayoutInfo.flags = 0;
		if (vkCreatePipelineLayout(*deviceHandle, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui pipeline layout");
		}

		// Pipeline
		// Create UI shader stages from compiled shader code
		auto uiVertShaderCode = ReadFile("shaders/ui.vert.spv");
		auto uiFragShaderCode = ReadFile("shaders/ui.frag.spv");

		// Create shader modules
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;
		vertShaderModule = CreateShaderModule(uiVertShaderCode);
		fragShaderModule = CreateShaderModule(uiFragShaderCode);

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
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

		// Set up vertex input format for imgui
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(ImDrawVert);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(ImDrawVert, pos);
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(ImDrawVert, uv);
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
		attributeDescriptions[2].offset = offsetof(ImDrawVert, col);
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
		rasterizer.cullMode = VK_CULL_MODE_NONE; // Cull back faces
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Faces are drawn counter clockwise to be considered front-facing
		rasterizer.depthBiasEnable = VK_FALSE;

		// Set up depth test
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_FALSE;
		depthStencil.depthWriteEnable = VK_FALSE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		// Set up multisampling (disabled)
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Color blend states
		VkPipelineColorBlendAttachmentState blendAttachmentState = {};
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo colourBlending = {};
		colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colourBlending.attachmentCount = 1;
		colourBlending.pAttachments = &blendAttachmentState;

		// Dynamic State
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = SCAST_U32(dynamicStateEnables.size());
		dynamicState.flags = 0;

		// We now have everything we need to create the pipeline object
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0; // Index of the sub pass where this pipeline will be used
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.stageCount = SCAST_U32(shaderStages.size());
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colourBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		// Now create the vis buff write pass pipeline
		if (vkCreateGraphicsPipelines(*deviceHandle, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui pipeline");
		}

		// Clean up shader module objects
		vkDestroyShaderModule(*deviceHandle, vertShaderModule, nullptr);
		vkDestroyShaderModule(*deviceHandle, fragShaderModule, nullptr);
	}

	// Define UI elements to display
	void ImGUI::UpdateFrame()
	{
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(200, 200));
		ImGui::Begin("Example settings");
		ImGui::Checkbox("Check box", &checkboxTest);
		ImGui::Checkbox("Check box", &checkboxTest);
		ImGui::Checkbox("Check box", &checkboxTest);
		ImGui::End();

		// Render to buffers
		ImGui::EndFrame();
		ImGui::Render();
	}

	void ImGUI::UpdateBuffers()
	{
		ImDrawData* imDrawData = ImGui::GetDrawData();

		VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
		VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

		if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
			return;
		}

		// Update buffers only if vertex or index count has changed
		// Vertex buffer
		if ((vertexBuffer.VkHandle() == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) 
		{
			vertexBuffer.CleanUp(*allocatorHandle);
			vertexBuffer.Create(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, *allocatorHandle);
			vertexCount = imDrawData->TotalVtxCount;
			vertexBuffer.Unmap(*allocatorHandle);
			vertexBuffer.Map(*allocatorHandle);
		}
		// Index buffer
		VkDeviceSize indexSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);
		if ((indexBuffer.VkHandle() == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) 
		{
			indexBuffer.CleanUp(*allocatorHandle);
			indexBuffer.Create(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, *allocatorHandle);
			indexCount = imDrawData->TotalIdxCount;
			indexBuffer.Map(*allocatorHandle);
		}

		// Copy data to mapped buffer ranges
		ImDrawVert* vtxMappedRange = (ImDrawVert*)vertexBuffer.mappedRange;
		ImDrawIdx* idxMappedRange = (ImDrawIdx*)indexBuffer.mappedRange;
		for (int i = 0; i < imDrawData->CmdListsCount; i++)
		{
			const ImDrawList* cmdList = imDrawData->CmdLists[i];
			//vertexBuffer.MapData(cmdList->VtxBuffer.Data, *allocatorHandle);
			//indexBuffer.MapData(cmdList->IdxBuffer.Data, *allocatorHandle);
			memcpy(vtxMappedRange, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxMappedRange, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtxMappedRange += cmdList->VtxBuffer.Size;
			idxMappedRange += cmdList->IdxBuffer.Size;
		}

		// Flush to make writes visible to GPU
		vertexBuffer.Flush(*allocatorHandle);
		indexBuffer.Flush(*allocatorHandle);
	}

	void ImGUI::DrawFrame(VkCommandBuffer commandBuffer)
	{
		ImGuiIO& io = ImGui::GetIO();

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkViewport viewport = {};
		viewport.width = ImGui::GetIO().DisplaySize.x;
		viewport.height = ImGui::GetIO().DisplaySize.y;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// UI scale and translate via push constants
		renderParamsPushConstant.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		renderParamsPushConstant.translate = glm::vec2(-1.0f);
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RenderParamsPushConstant), &renderParamsPushConstant);

		// Render commands
		ImDrawData* imDrawData = ImGui::GetDrawData();
		int32_t vertexOffset = 0;
		int32_t indexOffset = 0;

		if (imDrawData->CmdListsCount > 0) {

			VkDeviceSize offsets[1] = { 0 };
			VkBuffer vertexBuffers[] = { vertexBuffer.VkHandle() };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer.VkHandle(), 0, VK_INDEX_TYPE_UINT16);

			for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
			{
				const ImDrawList* cmdList = imDrawData->CmdLists[i];
				for (int32_t j = 0; j < cmdList->CmdBuffer.Size; j++)
				{
					const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];
					VkRect2D scissorRect;
					scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
					scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
					scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
					scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
					vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
					vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
					indexOffset += pcmd->ElemCount;
				}
				vertexOffset += cmdList->VtxBuffer.Size;
			}
		}
	}

	void ImGUI::CleanUp()
	{
		ImGui::DestroyContext();

		// Clean up vulkan objects
		fontImage.CleanUp(*allocatorHandle, *deviceHandle);
		vertexBuffer.CleanUp(*allocatorHandle);
		indexBuffer.CleanUp(*allocatorHandle);
		vkDestroyPipelineCache(*deviceHandle, pipelineCache, nullptr);
		vkDestroyPipeline(*deviceHandle, pipeline, nullptr);
		vkDestroyPipelineLayout(*deviceHandle, pipelineLayout, nullptr);
		vkDestroyDescriptorPool(*deviceHandle, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(*deviceHandle, descriptorLayout, nullptr);
	}

	VkShaderModule ImGUI::CreateShaderModule(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(*deviceHandle, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create ImGui shader module");
		}

		return shaderModule;
	}
}