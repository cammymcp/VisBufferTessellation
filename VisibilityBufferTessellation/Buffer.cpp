#include "Buffer.h"
#include <stdexcept>

namespace vbt
{
	void Buffer::Create(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage allocUsage, VkMemoryPropertyFlags properties, VmaAllocator& allocator)
	{
		// Store info
		bufferSize = size;
		usageFlags = usage;
		allocationUsage = allocUsage;
		propertyFlags = properties;

		// Fill buffer create info
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = usageFlags;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		// Specify memory allocation Info
		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = allocationUsage;
		allocInfo.requiredFlags = propertyFlags;

		// Create buffer
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &bufferMemory, nullptr) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create vertex buffer");
		}
	}

	void Buffer::MapData(void* data, VmaAllocator& allocator)
	{
		// Map data to staging buffer memory allocation
		void* mappedData;
		vmaMapMemory(allocator, bufferMemory, &mappedData);
		memcpy(mappedData, data, (size_t)bufferSize);
		vmaUnmapMemory(allocator, bufferMemory);
	}

	void Buffer::SetupDescriptor(VkDeviceSize size, VkDeviceSize offset)
	{
		descriptor.offset = offset;
		descriptor.buffer = buffer;
		descriptor.range = size;
	}

	void Buffer::CleanUp(VmaAllocator& allocator)
	{
		// Free memory and destroy buffer object
		vmaDestroyBuffer(allocator, buffer, bufferMemory);
	}
}