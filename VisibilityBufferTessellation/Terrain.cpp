#include "Terrain.h"

namespace vbt
{
	void Terrain::Generate(glm::vec3 origin, float width, float length)
	{
		// Create a unit quad for now
		Vertex vertex;
		VertexAttributes attributes;

		vertex = { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } };
		vertices.push_back(vertex);
		attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
		attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
		vertexAttributeData.push_back(attributes);

		vertex = { { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } };
		vertices.push_back(vertex);
		attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
		attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
		vertexAttributeData.push_back(attributes);

		vertex = { { 0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } };
		vertices.push_back(vertex);
		attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
		attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
		vertexAttributeData.push_back(attributes);

		vertex = { { -0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } };
		vertices.push_back(vertex);
		attributes.posXYZnormX = { vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.normal.x };
		attributes.normYZtexXY = { vertex.normal.y, vertex.normal.z, vertex.uv.x, vertex.uv.y };
		vertexAttributeData.push_back(attributes);

		indices = { 0, 1, 2, 0, 2, 3 };
	}
}