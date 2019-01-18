#include "Camera.h"

namespace vbt
{
	void Camera::Update(float frameTime)
	{
		updated = false;
		if (IsMoving())
		{
			glm::vec3 forward;
			forward.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
			forward.y = sin(glm::radians(rotation.x));
			forward.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
			forward = glm::normalize(forward);

			float speed = frameTime * moveSpeed;

			if (input.forward)
				position += forward * speed;
			if (input.back)
				position -= forward * speed;
			if (input.left)
				position -= glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f))) * speed;
			if (input.right)
				position += glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f))) * speed;
			if (input.up)
				position -= glm::normalize(glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f))) * speed;
			if (input.down)
				position += glm::normalize(glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f))) * speed;

			UpdateViewMatrix();
		}
	}

	void Camera::UpdateViewMatrix()
	{
		glm::mat4 rotationMatrix = glm::mat4(1.0f);
		glm::mat4 translationMatrix;

		// Perform per-axis rotation
		rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		//rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // Commented out to lock roll

		// Build translation matrix
		translationMatrix = glm::translate(glm::mat4(1.0f), position);

		viewMatrix = rotationMatrix * translationMatrix;

		updated = true;
	}

	void Camera::SetPerspective(float fov, float aspect, float nearPlane, float farPlane, bool setAsDefault)
	{
		this->fov = fov;
		this->nearPlane = nearPlane;
		this->farPlane = farPlane;
		projMatrix = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
		if (setAsDefault) defaultProjMatrix = projMatrix;
	}

	void Camera::SetPosition(glm::vec3 pos, bool setAsDefault)
	{
		position = pos;
		if (setAsDefault) defaultPosition = position;
		UpdateViewMatrix();
	}

	void Camera::SetRotation(glm::vec3 rot, bool setAsDefault)
	{
		rotation = rot;
		if (setAsDefault) defaultRotation = rotation;
		UpdateViewMatrix();
	}

	void Camera::Rotate(glm::vec3 delta)
	{
		rotation += delta;
		UpdateViewMatrix();
	}

	void Camera::Translate(glm::vec3 delta)
	{
		position += delta;
		UpdateViewMatrix();
	}

	void Camera::Reset()
	{
		position = defaultPosition;
		rotation = defaultRotation;
		projMatrix = defaultProjMatrix;
		UpdateViewMatrix();
	}
	
	bool Camera::IsMoving()
	{
		return input.forward || input.back || input.left || input.right || input.up || input.down;
	}
}