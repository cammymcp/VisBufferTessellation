#ifndef CAMERA_H
#define CAMERA_H

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm\glm.hpp"
#include "glm\gtc\matrix_transform.hpp"
#include "glm\gtc\quaternion.hpp"

namespace vbt
{
	class Camera
	{
	public:
		void Update(float frameTime);
		void SetPerspective(float fov, float aspect, float nearPlane, float farPlane);
		void SetPosition(glm::vec3 pos);
		void SetRotation(glm::vec3 rot);
		void Rotate(glm::vec3 delta);
		void Translate(glm::vec3 delta);

		glm::mat4 ViewMatrix() const { return viewMatrix; }
		glm::mat4 ProjectionMatrix() const { return projMatrix; }

		float rotateSpeed = 1.0f;
		float moveSpeed = 1.0f;

		bool updated = false;

		struct
		{
			bool forward = false;
			bool back = false;
			bool left = false;
			bool right = false;
		} input;

	private:
		void UpdateViewMatrix();
		bool IsMoving();

		glm::mat4 projMatrix;
		glm::mat4 viewMatrix;

		glm::vec3 position = glm::vec3();
		glm::vec3 rotation = glm::vec3();
		float fov;
		float nearPlane, farPlane;
	};
}

#endif