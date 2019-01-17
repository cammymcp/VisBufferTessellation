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
}