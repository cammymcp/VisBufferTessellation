#ifndef TERRAINPATCH_H
#define TERRAINPATCH_H

#include "Mesh.h"
#include "glm\glm.hpp"

namespace vbt
{
	class Terrain : public Mesh
	{
	public:
		void Generate(glm::vec3 origin, float width, float length);
	};
}

#endif