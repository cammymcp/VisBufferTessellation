#ifndef DIRECTIONALLIGHT_H
#define DIRECTIONALLIGHT_H

#include <glm/glm.hpp>
#include "Buffer.h"

struct DirectionalLightUBO
{
	glm::vec4 direction;
	glm::vec4 ambient;
	glm::vec4 diffuse;
};

namespace vbt
{
	class DirectionalLight
	{
	public:
		struct InitInfo
		{
			glm::vec4 direction;
			glm::vec4 ambient;
			glm::vec4 diffuse;
		};

		void Init(InitInfo info, VmaAllocator& allocator);
		void UpdateUBO(VmaAllocator& allocator);
		void SetupUBODescriptors(VkDescriptorSet dstSet, uint32_t binding, uint32_t count);
		void CleanUp(VmaAllocator& allocator);
		vbt::Buffer UBO() { return ubo; }
		glm::vec4 Direction() { return direction; }
		glm::vec4 Ambient() { return ambient; }
		glm::vec4 Diffuse() { return diffuse; }
		void SetDirection(glm::vec4 dir) { direction = dir; }
		void SetAmbient(glm::vec4 amb) { ambient = amb; }
		void SetDiffuse(glm::vec4 diff) { diffuse = diff; }

	private:
		glm::vec4 direction;
		glm::vec4 ambient;
		glm::vec4 diffuse;

		vbt::Buffer ubo;
	};
}
#endif 