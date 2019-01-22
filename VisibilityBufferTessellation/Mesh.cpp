#include "Mesh.h"
#include "VbtUtils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace vbt
{
	void Mesh::LoadFromFile(std::string path)
	{
		tinyobj::attrib_t attribute;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;

		// LoadObj automatically triangulates faces by default, so we don't need to worry about that 
		if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &err, path.c_str()))
		{
			throw std::runtime_error(err);
		}

		// Use a map to track the index of unique vertices
		std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

		// Combine all shapes into one model
		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				Vertex vertex = {};
				VertexAttributes vertexAttributes = {}; // For storing geometry attributes only to be accessed by shading pass. Position, normal and tex are packed into 2 vec4s

				// Vertices array is an array of floats, so we have to multiply the index by three each time, and offset by 0/1/2 to get the X/Y/Z components
				vertex.pos =
				{
					attribute.vertices[3 * index.vertex_index + 0],
					attribute.vertices[3 * index.vertex_index + 1],
					attribute.vertices[3 * index.vertex_index + 2]
				};
				vertexAttributes.posXYZnormX.x = vertex.pos.x;
				vertexAttributes.posXYZnormX.y = vertex.pos.y;
				vertexAttributes.posXYZnormX.z = vertex.pos.z;

				vertex.normal = { 1.0f, 1.0f, 1.0f };
				vertexAttributes.posXYZnormX.w = vertex.normal.x;
				vertexAttributes.normYZtexXY.x = vertex.normal.y;
				vertexAttributes.normYZtexXY.y = vertex.normal.z;

				vertex.uv =
				{
					attribute.texcoords[2 * index.texcoord_index + 0],
					1.0f - attribute.texcoords[2 * index.texcoord_index + 1] // Flip texture Y coordinate to match vulkan coord system
				};
				vertexAttributes.normYZtexXY.z = vertex.uv.x;
				vertexAttributes.normYZtexXY.w = vertex.uv.y;

				// Check if we've already seen this vertex before, and add it to the vertex buffer if not
				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = SCAST_U32(vertices.size());
					vertices.push_back(vertex);
					vertexAttributeData.push_back(vertexAttributes);
				}
				indices.push_back(uniqueVertices[vertex]);
			}
		}
	}

	void Mesh::SetupIndexBufferDescriptor(VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count)
	{
		indexBuffer.SetupDescriptor(sizeof(indices[0]) * indices.size(), 0);
		indexBuffer.SetupDescriptorWriteSet(dstSet, binding, type, count);
	}

	void Mesh::SetupAttributeBufferDescriptor(VkDescriptorSet dstSet, uint32_t binding, VkDescriptorType type, uint32_t count)
	{
		attributeBuffer.SetupDescriptor(sizeof(vertexAttributeData[0]) * vertexAttributeData.size(), 0);
		attributeBuffer.SetupDescriptorWriteSet(dstSet, binding, type, count);
	}

	void Mesh::CleanUp(VmaAllocator& allocator)
	{
		vertexBuffer.CleanUp(allocator);
		indexBuffer.CleanUp(allocator);
		attributeBuffer.CleanUp(allocator);
	}

	void Mesh::CreateBuffers(VmaAllocator& allocator, VkDevice device, PhysicalDevice physDevice, VkCommandPool& cmdPool)
	{
		// Vertex Buffer
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		Buffer stagingBuffer;
		stagingBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

		// Map vertex data to staging buffer memory allocation
		stagingBuffer.MapData(vertices.data(), allocator);

		// Create vertex buffer on device local memory
		vertexBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

		// Copy data to new vertex buffer
		CopyBuffer(stagingBuffer.VkHandle(), vertexBuffer.VkHandle(), bufferSize, device, physDevice, cmdPool);

		stagingBuffer.CleanUp(allocator);

		// Index Buffer
		bufferSize = sizeof(indices[0]) * indices.size();
		stagingBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

		stagingBuffer.MapData(indices.data(), allocator);

		indexBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

		CopyBuffer(stagingBuffer.VkHandle(), indexBuffer.VkHandle(), bufferSize, device, physDevice, cmdPool);

		stagingBuffer.CleanUp(allocator);

		// Attribute Buffer
		bufferSize = sizeof(vertexAttributeData[0]) * vertexAttributeData.size();
		stagingBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator);

		stagingBuffer.MapData(vertexAttributeData.data(), allocator);

		attributeBuffer.Create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator);

		CopyBuffer(stagingBuffer.VkHandle(), attributeBuffer.VkHandle(), bufferSize, device, physDevice, cmdPool);

		// Clean up staging buffer
		stagingBuffer.CleanUp(allocator);
	}
}