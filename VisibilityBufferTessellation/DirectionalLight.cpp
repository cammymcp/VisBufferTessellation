#include "DirectionalLight.h"

namespace vbt
{
	void DirectionalLight::Init(InitInfo info, VmaAllocator& allocator)
	{
		diffuse = info.diffuse;
		direction = info.direction;
		ambient = info.ambient;

		// Setup UBO
		VkDeviceSize bufferSize = sizeof(DirectionalLightUBO);
		DirectionalLightUBO uboData = {};
		uboData.diffuse = diffuse;
		uboData.direction = direction;
		uboData.ambient = ambient;

		ubo.Create(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);
		ubo.MapData(&uboData, allocator);
	}

	void DirectionalLight::CleanUp(VmaAllocator& allocator)
	{
		ubo.CleanUp(allocator);
	}

	void DirectionalLight::SetupUBODescriptors(VkDescriptorSet dstSet, uint32_t binding, uint32_t count)
	{
		ubo.SetupDescriptor(sizeof(DirectionalLightUBO), 0);
		ubo.SetupDescriptorWriteSet(dstSet, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count);
	}

	void DirectionalLight::UpdateUBO(VmaAllocator& allocator)
	{
		DirectionalLightUBO uboData = {};
		uboData.diffuse = diffuse;
		uboData.direction = direction;
		uboData.ambient = ambient;

		ubo.MapData(&uboData, allocator);
	}
}
