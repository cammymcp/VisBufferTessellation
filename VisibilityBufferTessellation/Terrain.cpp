#include "Terrain.h"

namespace vbt
{
	void Terrain::Generate(glm::vec3 origin, float width, float length)
	{
		// Generate vertices
		for (auto x = 0; x < VERTEX_PER_SIDE; x++)
		{
			for (auto y = 0; y < VERTEX_PER_SIDE; y++)
			{
				Vertex vertex;
				VertexAttributes attributes;
				uint32_t index = (x + y * VERTEX_PER_SIDE);
				vertex.pos.x = x * VERTEX_OFFSET + VERTEX_OFFSET / 2.0f - (float)VERTEX_PER_SIDE * VERTEX_OFFSET / 2.0f;
				vertex.pos.y = y * VERTEX_OFFSET + VERTEX_OFFSET / 2.0f - (float)VERTEX_PER_SIDE * VERTEX_OFFSET / 2.0f;
				vertex.pos.z = 0;
				vertex.uv = glm::vec2((float)x / VERTEX_PER_SIDE, (float)y / VERTEX_PER_SIDE) * -UV_SCALE;
				vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
				attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
				attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
				vertices.push_back(vertex);
				vertexAttributeData.push_back(attributes);
			}
		}

		// Generate Indices
		const uint32_t w = VERTEX_PER_SIDE - 1;
		indices.resize(w * w * 3);
		for (auto x = 0; x < w; x++)
		{
			for (auto y = 0; y < w; y++)
			{
				uint32_t index = (x + y * w) * 3;
				indices[index] = (x + y * VERTEX_PER_SIDE);
				indices[index + 1] = indices[index] + VERTEX_PER_SIDE;
				indices[index + 2] = indices[index + 1] + 1;
			}
		}
	}
}