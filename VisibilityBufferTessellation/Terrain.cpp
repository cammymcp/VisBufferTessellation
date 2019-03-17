#include "Terrain.h"
#include "VbtUtils.h"

namespace vbt
{
	// Generates terrain mesh, loads textures and returns triangle count. 
	int Terrain::Init(VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool, InitInfo info)
	{
		texture.LoadAndCreate(TEXTURE_PATH, allocator, device, physDevice, cmdPool);
		heightmap.LoadAndCreate(HEIGHTMAP_PATH, allocator, device, physDevice, cmdPool);
		normalmap.LoadAndCreate(NORMALMAP_PATH, allocator, device, physDevice, cmdPool);

		int triangleCount = Generate(info.subdivisions, info.width, info.uvScale);

		CreateBuffers(allocator, device, physDevice, cmdPool);

		return triangleCount;
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

	void Terrain::SetupNormalmapDescriptor(VkImageLayout layout, VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count)
	{
		normalmap.SetUpDescriptorInfo(layout);
		normalmap.SetupDescriptorWriteSet(dstSet, binding, type, count);
	}

	void Terrain::CleanUp(VmaAllocator& allocator, VkDevice device)
	{
		this->Mesh::CleanUp(allocator);
		texture.CleanUp(allocator, device);
		heightmap.CleanUp(allocator, device);
		normalmap.CleanUp(allocator, device);
	}

	int Terrain::Generate(int verticesPerEdge, int width, float uvScale)
	{
		// Get triangle count
		const uint32_t quadsPerSide = verticesPerEdge - 1;
		int quadCount = quadsPerSide * quadsPerSide;
		int triangleCount = quadCount * 2;

		// Get offset from width
		float vertexOffset = (float)width / (float)(verticesPerEdge - 1);

		// Generate vertices
		for (auto x = 0; x < verticesPerEdge; x++)
		{
			for (auto z = 0; z < verticesPerEdge; z++)
			{
				Vertex vertex;
				VertexAttributes attributes;
				vertex.pos.x = x * vertexOffset + vertexOffset / 2.0f - (float)verticesPerEdge * vertexOffset / 2.0f;
				vertex.pos.y = 0;
				vertex.pos.z = z * vertexOffset + vertexOffset / 2.0f - (float)verticesPerEdge * vertexOffset / 2.0f;
				vertex.uv = glm::vec2((float)x / verticesPerEdge, (float)z / verticesPerEdge) * -uvScale;
				vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
				attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
				attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
				vertices.push_back(vertex);
				vertexAttributeData.push_back(attributes);
			}
		}
		
		// Generate triangle list indices
		indices.resize(quadCount * 6);
		for (auto x = 0; x < quadsPerSide; x++)
		{
			for (auto y = 0; y < quadsPerSide; y++)
			{
				uint32_t index = (x + y * quadsPerSide) * 6;
				indices[index] = (x + y * verticesPerEdge); // bottom left
				indices[index + 1] = indices[index] + verticesPerEdge; // bottom right
				indices[index + 2] = indices[index + 1] + 1; // top right
				indices[index + 3] = indices[index] + 1; // top left
				indices[index + 4] = (x + y * verticesPerEdge); // bottom left
				indices[index + 5] = indices[index + 1] + 1; // top right
			}
		}

		return triangleCount;
	}
}