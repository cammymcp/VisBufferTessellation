#ifndef TEXTURE_H
#define TEXTURE_H

#include "vk_mem_alloc.h"
#include "Image.h"

namespace vbt
{
	// At the moment just a wrapper class for Image to allow loading data from file, may want to extend later
	class Texture: public Image
	{
	public:
		void LoadAndCreate(std::string path, VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
	};
}

#endif