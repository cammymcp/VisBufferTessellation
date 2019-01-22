#include "Terrain.h"

namespace vbt
{
	void Terrain::Generate(glm::vec3 origin, float width, float length)
	{
		// Generate vertices
		for (auto x = 0; x < QUAD_PER_SIDE; x++)
		{
			for (auto z = 0; z < QUAD_PER_SIDE; z++)
			{
				Vertex vertex;
				VertexAttributes attributes;
				vertex.pos.x = x * VERTEX_OFFSET + VERTEX_OFFSET / 2.0f - (float)QUAD_PER_SIDE * VERTEX_OFFSET / 2.0f;
				vertex.pos.y = 0;
				vertex.pos.z = z * VERTEX_OFFSET + VERTEX_OFFSET / 2.0f - (float)QUAD_PER_SIDE * VERTEX_OFFSET / 2.0f;
				vertex.uv = glm::vec2((float)x / QUAD_PER_SIDE, (float)z / QUAD_PER_SIDE) * -UV_SCALE;
				vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
				attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
				attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
				vertices.push_back(vertex);
				vertexAttributeData.push_back(attributes);
			}
		}
		
		// Generate Indices
		const uint32_t patchWidth = QUAD_PER_SIDE - 1;
		indices.resize(patchWidth * patchWidth * 6);
		for (auto x = 0; x < patchWidth; x++)
		{
			for (auto y = 0; y < patchWidth; y++)
			{
				uint32_t index = (x + y * patchWidth) * 6;
				indices[index] = (x + y * QUAD_PER_SIDE); // bottom left
				indices[index + 1] = indices[index] + QUAD_PER_SIDE; // bottom right
				indices[index + 2] = indices[index + 1] + 1; // top right
				indices[index + 3] = indices[index] + 1; // top left
				indices[index + 4] = (x + y * QUAD_PER_SIDE); // bottom left
				indices[index + 5] = indices[index + 1] + 1; // top right
			}
		}
	}
}