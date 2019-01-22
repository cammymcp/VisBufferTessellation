#ifndef TERRAINPATCH_H
#define TERRAINPATCH_H

#include "Mesh.h"
#include "Texture.h"
#include "Buffer.h"
#include "glm\glm.hpp"

#define QUAD_PER_SIDE 512
#define VERTEX_OFFSET 0.1f
#define UV_SCALE 10.0f

const std::string TEXTURE_PATH = "textures/grid.png";

namespace vbt
{
	class Terrain : public Mesh
	{
	public:
		void Init(VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool);
		void CleanUp(VmaAllocator& allocator, VkDevice device);
		Texture GetTexture() { return texture; } 

	private:
		void Generate();

		Texture texture;
	};
}

#endif