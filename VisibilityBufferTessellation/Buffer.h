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
		void CleanUp(VmaAllocator& allocator);

		VkBuffer VkHandle() { return buffer; }
		VkDescriptorBufferInfo* DescriptorInfo() { return &descriptor; }

	protected:
		VkBuffer buffer;
		VmaAllocation bufferMemory;
		VkDeviceSize bufferSize;

		VkBufferUsageFlags usageFlags;
		VkMemoryPropertyFlags propertyFlags;
		VmaMemoryUsage allocationUsage; 
		VkDescriptorBufferInfo descriptor;
	};
}

#endif