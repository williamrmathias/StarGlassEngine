// sge
#include "Camera.h"

// glm
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position, position + front, up);
}

glm::vec3 Camera::getViewPosition() const
{
	return position;
}

void Camera::processSDLEvent(SDL_Event& event, Uint32 mouseState)
{
	if (event.type == SDL_KEYDOWN)
	{
		// NOTE: Opposite keys overwrite each other. I don't know if I like it or not
		if (event.key.keysym.sym == SDLK_w) { velocity.z = -1; }
		if (event.key.keysym.sym == SDLK_s) { velocity.z = 1; }
		if (event.key.keysym.sym == SDLK_a) { velocity.x = 1; }
		if (event.key.keysym.sym == SDLK_d) { velocity.x = -1; }
		if (event.key.keysym.sym == SDLK_e) { velocity.y = -1; }
		if (event.key.keysym.sym == SDLK_q) { velocity.y = 1; }
	}

	if (event.type == SDL_KEYUP)
	{
		if (event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_s) { velocity.z = 0; }
		if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_d) { velocity.x = 0; }
		if (event.key.keysym.sym == SDLK_e || event.key.keysym.sym == SDLK_q) { velocity.y = 0; }
	}

	if ((mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) && event.type == SDL_MOUSEMOTION)
	{
		yaw += static_cast<float>(event.motion.xrel) * sensitivity;
		pitch -= static_cast<float>(event.motion.yrel) * sensitivity;
		pitch = glm::clamp(pitch, -89.f, 89.f);

		updateVectors();
	}
}

void Camera::updateVectors()
{
	float yawRad = glm::radians(yaw);
	float pitchRad = glm::radians(pitch);

	float cosPitch = glm::cos(pitchRad);

	front = {
		glm::cos(yawRad) * cosPitch,
		glm::sin(pitchRad),
		glm::sin(yawRad) * cosPitch
	};

	right = glm::normalize(glm::cross(front, worldUp));
	up = glm::normalize(glm::cross(right, front));
}

void Camera::updatePosition(float deltaTime)
{
	float distance = speed * deltaTime;

	position += front * velocity.z * distance;
	position += up * velocity.y * distance;
	position += right * velocity.x * distance;
}