#ifndef TERRAINPATCH_H
#define TERRAINPATCH_H

#include "Mesh.h"
#include "glm\glm.hpp"

#define QUAD_PER_SIDE 512
#define VERTEX_OFFSET 0.1f
#define UV_SCALE 10.0f

namespace vbt
{
	class Terrain : public Mesh
	{
	public:
		void Generate(glm::vec3 origin, float width, float length);
	};
}

#endif