#ifndef TERRAINPATCH_H
#define TERRAINPATCH_H

#include "Mesh.h"
#include "Texture.h"
#include "Buffer.h"
#include "glm\glm.hpp"

const std::string TEXTURE_PATH = "textures/rock.jpg";
const std::string HEIGHTMAP_PATH = "textures/heightmap.png";

namespace vbt
{
	class Terrain : public Mesh
	{
	public:
		struct InitInfo
		{
			int subdivisions = 64;
			int width = 32;
			float uvScale = 5.0f;

			InitInfo()
			{}
		};

		int Init(VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool, InitInfo info);
		void SetupTextureDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void SetupHeightmapDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count);
		void CleanUp(VmaAllocator& allocator, VkDevice device);

		Texture GetTexture() { return texture; } 
		Texture Heightmap() { return heightmap; }

	private:
		int Generate(int verticesPerEdge, int width, float uvScale);

		vbt::Texture texture;
		vbt::Texture heightmap; 
	};
}

#endif