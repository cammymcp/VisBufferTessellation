#ifndef BUFFER_H
#define BUFFER_H

#include "vk_mem_alloc.h"

namespace vbt
{
	class Buffer
	{
	public:
		void Create(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage allocUsage, VkMemoryPropertyFlags properties, VmaAllocator& allocator);
		void MapData(void* data, VmaAllocator& allocator);
		void SetupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void SetupDescriptorWriteSet(VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void CleanUp(VmaAllocator& allocator);

		VkBuffer VkHandle() { return buffer; }
		VkDescriptorBufferInfo* DescriptorInfo() { return &descriptor; }
		VkWriteDescriptorSet WriteDescriptorSet() { return descriptorWriteSet; }

	protected:
		VkBuffer buffer;
		VmaAllocation bufferMemory;
		VkDeviceSize bufferSize;

		VkBufferUsageFlags usageFlags;
		VkMemoryPropertyFlags propertyFlags;
		VmaMemoryUsage allocationUsage; 
		VkDescriptorBufferInfo descriptor; 
		VkWriteDescriptorSet descriptorWriteSet;
	};
}

#endif