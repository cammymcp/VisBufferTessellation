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

	void Buffer::Flush(VmaAllocator& allocator, VkDeviceSize size, VkDeviceSize offset)
	{
		vmaFlushAllocation(allocator, bufferMemory, offset, size);
	}

	void Buffer::Map(VmaAllocator& allocator)
	{
		vmaMapMemory(allocator, bufferMemory, &mappedRange);
	}

	void Buffer::Unmap(VmaAllocator& allocator)
	{
		if (mappedRange)
		{
			vmaUnmapMemory(allocator, bufferMemory);
			mappedRange = nullptr;
		}
	}

	void Buffer::SetupDescriptor(VkDeviceSize size, VkDeviceSize offset)
	{
		descriptor.offset = offset;
		descriptor.buffer = buffer;
		descriptor.range = size;
	}

	void Buffer::SetupDescriptorWriteSet(VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count)
	{
		descriptorWriteSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWriteSet.dstSet = dstSet;
		descriptorWriteSet.dstBinding = binding;
		descriptorWriteSet.dstArrayElement = 0;
		descriptorWriteSet.descriptorType = type;
		descriptorWriteSet.descriptorCount = count;
		descriptorWriteSet.pBufferInfo = &descriptor;
	}

	void Buffer::CleanUp(VmaAllocator& allocator)
	{
		// Free memory and destroy buffer object
		Unmap(allocator);
		vmaDestroyBuffer(allocator, buffer, bufferMemory);
	}
}