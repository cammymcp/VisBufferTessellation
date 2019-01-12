#ifndef IMAGE_H
#define IMAGE_H

#include "PhysicalDevice.h"
#include <vulkan\vulkan.h>
#include "vk_mem_alloc.h"

namespace vbt
{
	class Image
	{
	public:
		void Create(uint32_t imageWidth, uint32_t imageHeight, VkFormat imageFormat, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags properties, VmaAllocator& allocator);
		void CreateImageView(const VkDevice device, VkImageAspectFlags aspectFlags);
		void TransitionLayout(VkImageLayout srcLayout, VkImageLayout dstLayout, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
		void CopyFromBuffer(VkBuffer buffer, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
		void CleanUp(VmaAllocator& allocator, VkDevice device);

		VkImage VkHandle() { return image; }
		VkImageView ImageView() { return imageView; }
		VkFormat Format() { return format; }

	protected:
		bool HasStencilComponent(VkFormat format);

		VkImage image;
		VkImageLayout imageLayout;
		VkImageView imageView;
		VkFormat format;
		VmaAllocation imageMemory;

		uint32_t width, height = 0;
	};
}

#endif