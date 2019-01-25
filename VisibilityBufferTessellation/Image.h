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
		void CreateSampler(VkDevice device, VkSamplerAddressMode addressMode);
		void SetUpDescriptorInfo(VkImageLayout layout);
		void SetupDescriptorWriteSet(VkDescriptorSet& dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void TransitionLayout(VkImageLayout srcLayout, VkImageLayout dstLayout, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
		void CopyFromBuffer(VkBuffer buffer, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
		void CleanUp(VmaAllocator& allocator, VkDevice device);

		VkImage VkHandle() { return image; }
		VkImageView ImageView() { return imageView; }
		VkFormat Format() { return format; }
		VkSampler Sampler() { return sampler; }
		VkDescriptorImageInfo* DescriptorInfo() { return &descriptor; }
		VkWriteDescriptorSet WriteDescriptorSet() const { return writeDescriptorSet; }

	protected:
		bool HasStencilComponent(VkFormat format);

		VkImage image;
		VkImageLayout imageLayout;
		VkImageView imageView;
		VkFormat format;
		VmaAllocation imageMemory;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
		VkWriteDescriptorSet writeDescriptorSet;

		uint32_t width, height = 0;
	};
}

#endif