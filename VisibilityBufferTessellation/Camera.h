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
		void SetPerspective(float fov, float aspect, float nearPlane, float farPlane, bool setAsDefault = false);
		void SetPosition(glm::vec3 pos, bool setAsDefault = false);
		void SetRotation(glm::vec3 rot, bool setAsDefault = false);
		void Rotate(glm::vec3 delta);
		void Translate(glm::vec3 delta);
		void Reset();

		glm::vec3 Position() { return position; }
		glm::vec3 Rotation() { return rotation; }

		glm::mat4 ViewMatrix() const { return viewMatrix; }
		glm::mat4 ProjectionMatrix() const { return projMatrix; }

		float rotateSpeed = 0.25f;
		float moveSpeed = 1.0f;

		bool updated = false;

		struct
		{
			bool forward = false;
			bool back = false;
			bool left = false;
			bool right = false;
			bool up = false;
			bool down = false;
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

		// Used to reset camera to initial position
		glm::mat4 defaultProjMatrix; 
		glm::vec3 defaultPosition = glm::vec3();
		glm::vec3 defaultRotation = glm::vec3();
	};
}

#endif