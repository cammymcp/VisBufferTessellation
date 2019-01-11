#include "Texture.h"
#include "Buffer.h"
#include "HelperFunctions.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace vbt
{
	void Texture::Create(std::string path, VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool)
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
		image.Create(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

		// Transition the texture image to the correct layout for recieving the pixel data from the buffer
		image.TransitionLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, device, physDevice, cmdPool);

		// Execute the copy from buffer to image
		image.CopyFromBuffer(stagingBuffer.VkHandle(), device, physDevice, cmdPool);

		// Now transition the image layout again to allow for sampling in the shader
		image.TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, device, physDevice, cmdPool);

		// Free up the staging buffer
		stagingBuffer.CleanUp(allocator);

		// Now create the image view and the sampler
		image.CreateImageView(device, VK_IMAGE_ASPECT_COLOR_BIT);
		CreateSampler(device);
	}

	void Texture::CreateSampler(VkDevice device)
	{
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR; // Apply linear interpolation to over/under-sampled texels
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE; // Enable anisotropic filtering 
		samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE; // Clamp texel coordinates to [0, 1]
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create texture sampler");
		}
	}

	void Texture::CleanUp(VmaAllocator& allocator, VkDevice device)
	{
		vkDestroySampler(device, sampler, nullptr);
		image.CleanUp(allocator, device);
	}
}
