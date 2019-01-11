#ifndef TEXTURE_H
#define TEXTURE_H

#include "vk_mem_alloc.h"
#include "Image.h"

namespace vbt
{
	class Texture
	{
	public:
		void Create(std::string path, VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
		void CleanUp(VmaAllocator& allocator, VkDevice device);

		Image GetImage() { return image; }
		VkSampler Sampler() { return sampler; }

	protected:
		void CreateSampler(VkDevice device);

		Image image;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
	};
}

#endif