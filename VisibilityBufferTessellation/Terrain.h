#ifndef TERRAINPATCH_H
#define TERRAINPATCH_H

#include "Mesh.h"
#include "Texture.h"
#include "Buffer.h"
#include "glm\glm.hpp"

#define PATCH_SIZE 512
#define VERTEX_OFFSET 0.1f
#define UV_SCALE 1.0f

const std::string TEXTURE_PATH = "textures/grid.png";
const std::string HEIGHTMAP_PATH = "textures/heightmap.png";

namespace vbt
{
	class Terrain : public Mesh
	{
	public:
		void Init(VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
		void SetupTextureDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void SetupHeightmapDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void CleanUp(VmaAllocator& allocator, VkDevice device);

		Texture GetTexture() { return texture; } 
		Texture Heightmap() { return heightmap; }

	private:
		void Generate();

		Texture texture;
		Texture heightmap;
	};
}

#endif