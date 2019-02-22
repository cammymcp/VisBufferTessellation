#ifndef TERRAINPATCH_H
#define TERRAINPATCH_H

#include "Mesh.h"
#include "Texture.h"
#include "Buffer.h"
#include "glm\glm.hpp"

const std::string TEXTURE_PATH = "textures/grid.png";
const std::string HEIGHTMAP_PATH = "textures/heightmap.png";

namespace vbt
{
	class Terrain : public Mesh
	{
	public:
		struct InitInfo
		{
			int verticesPerEdge = 64;
			int width = 32;
			float uvScale = 5.0f;

			InitInfo()
			{}
		};

		void Init(VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool, InitInfo info);
		void SetupTextureDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void SetupHeightmapDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void CleanUp(VmaAllocator& allocator, VkDevice device);

		Texture GetTexture() { return texture; } 
		Texture Heightmap() { return heightmap; }

	private:
		void Generate(int verticesPerEdge, int width, float uvScale);

		Texture texture;
		Texture heightmap;
	};
}

#endif