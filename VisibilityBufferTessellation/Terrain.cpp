#include "Terrain.h"
#include "VbtUtils.h"

namespace vbt
{
	void Terrain::Init(VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool, bool tessellated)
	{
		texture.LoadAndCreate(TEXTURE_PATH, allocator, device, physDevice, cmdPool);
		heightmap.LoadAndCreate(HEIGHTMAP_PATH, allocator, device, physDevice, cmdPool);

		Generate(tessellated);

		CreateBuffers(allocator, device, physDevice, cmdPool);
	}

	void Terrain::SetupTextureDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count)
	{
		texture.SetUpDescriptorInfo(layout);
		texture.SetupDescriptorWriteSet(dstSet, binding, type, count);
	}

	void Terrain::SetupHeightmapDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count)
	{
		heightmap.SetUpDescriptorInfo(layout);
		heightmap.SetupDescriptorWriteSet(dstSet, binding, type, count);
	}

	void Terrain::CleanUp(VmaAllocator& allocator, VkDevice device)
	{
		this->Mesh::CleanUp(allocator);
		texture.CleanUp(allocator, device);
		heightmap.CleanUp(allocator, device);
	}

	void Terrain::Generate(bool tessellated)
	{
		// Generate vertices
		for (auto x = 0; x < PATCH_SIZE; x++)
		{
			for (auto z = 0; z < PATCH_SIZE; z++)
			{
				Vertex vertex;
				VertexAttributes attributes;
				vertex.pos.x = x * VERTEX_OFFSET + VERTEX_OFFSET / 2.0f - (float)PATCH_SIZE * VERTEX_OFFSET / 2.0f;
				vertex.pos.y = 0;
				vertex.pos.z = z * VERTEX_OFFSET + VERTEX_OFFSET / 2.0f - (float)PATCH_SIZE * VERTEX_OFFSET / 2.0f;
				vertex.uv = glm::vec2((float)x / PATCH_SIZE, (float)z / PATCH_SIZE) * -UV_SCALE;
				vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
				attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
				attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
				vertices.push_back(vertex);
				vertexAttributeData.push_back(attributes);
			}
		}
		
		// Generate indices depending on whether this terrain is set to be tessellated 
		if (!tessellated)
		{
			// Triangle list vertices
			const uint32_t patchWidth = PATCH_SIZE - 1;
			indices.resize(patchWidth * patchWidth * 6);
			for (auto x = 0; x < patchWidth; x++)
			{
				for (auto y = 0; y < patchWidth; y++)
				{
					uint32_t index = (x + y * patchWidth) * 6;
					indices[index] = (x + y * PATCH_SIZE); // bottom left
					indices[index + 1] = indices[index] + PATCH_SIZE; // bottom right
					indices[index + 2] = indices[index + 1] + 1; // top right
					indices[index + 3] = indices[index] + 1; // top left
					indices[index + 4] = (x + y * PATCH_SIZE); // bottom left
					indices[index + 5] = indices[index + 1] + 1; // top right
				}
			}
		}
		else
		{
			// Quad tessellation patches
			const uint32_t w = (PATCH_SIZE - 1);
			const uint32_t indexCount = w * w * 4;
			indices.resize(indexCount);
			for (auto x = 0; x < w; x++)
			{
				for (auto y = 0; y < w; y++)
				{
					uint32_t index = (x + y * w) * 4;
					indices[index] = (x + y * PATCH_SIZE);
					indices[index + 1] = indices[index] + PATCH_SIZE;
					indices[index + 2] = indices[index + 1] + 1;
					indices[index + 3] = indices[index] + 1;
				}
			}
		}
	}
}