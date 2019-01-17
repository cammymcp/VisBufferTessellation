#ifndef MESH_H
#define MESH_H

#include "Buffer.h"
#include "vk_mem_alloc.h"
#include <cstdlib>
#include <array>
#include <glm\glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#pragma region Vertex Data
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	} 

	// Create an attribute description PER ATTRIBUTE (currently pos normal and texcoords)
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, uv);

		return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && normal == other.normal && uv == other.uv;
	}
};

// Hash calculation for Vertex struct 
namespace std
{
	template<> struct hash<Vertex>
	{
		size_t operator()(Vertex const& vertex) const
		{
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}

// 16-byte aligned vertex attributes. 
struct VertexAttributes
{
	glm::vec4 posXYZnormX;
	glm::vec4 normYZtexXY;
};
#pragma endregion

namespace vbt
{
	class Mesh
	{
	public:
		void LoadFromFile(std::string path);

		std::vector<Vertex> Vertices() const { return vertices; }
		std::vector<uint32_t> Indices() const { return indices; }
		std::vector<VertexAttributes> PackedVertexAttributes() const { return vertexAttributeData; }
		
	protected:
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<VertexAttributes> vertexAttributeData;
	};
}

#endif

