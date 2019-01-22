#include "Texture.h"
#include "Buffer.h"
#include "VbtUtils.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace vbt
{
	void Texture::LoadAndCreate(std::string path, VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool)
	{
		// Load image file with STB library
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4;
		if (!pixels)
		{
			throw std::runtime_error("Failed to load texture image");
		}

		// Create a staging buffer for the pixel data
		Buffer stagingBuffer;
		stagingBuffer.Create(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

		// Copy the pixel values directly to the buffer
		stagingBuffer.MapData(pixels, allocator);

		// Clean up pixel memory
		stbi_image_free(pixels);

		// Now create Vulkan Image object
		Create(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

		// Transition the texture image to the correct layout for recieving the pixel data from the buffer
		TransitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, device, physDevice, cmdPool);

		// Execute the copy from buffer to image
		CopyFromBuffer(stagingBuffer.VkHandle(), device, physDevice, cmdPool);

		// Now transition the image layout again to allow for sampling in the shader
		TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, device, physDevice, cmdPool);

		// Free up the staging buffer
		stagingBuffer.CleanUp(allocator);

		// Now create the image view and the sampler
		CreateImageView(device, VK_IMAGE_ASPECT_COLOR_BIT);
		CreateSampler(device);
	}
}
