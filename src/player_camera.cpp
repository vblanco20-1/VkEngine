#pragma once
#include "player_camera.h"

#include <glm/gtx/rotate_vector.hpp>

void PlayerCamera::rebuild_matrix()
{
	glm::vec3 side = glm::cross(camera_location, camera_up);

	glm::vec3 offset = glm::vec3(0); //playerCam.camera_up * ry + side * rx;

	view_matrix = glm::lookAt(camera_location, camera_location + camera_forward, camera_up);
}

void PlayerCamera::update(glm::vec3 movement, glm::vec2 rotation, float deltaTime)
{
	//horizontal rotation
	camera_forward = glm::rotate(camera_forward, rotation.x * deltaTime, camera_up);

	glm::vec3 right_direction = glm::cross(camera_up, camera_forward);

	//vertical rotation
	camera_forward = glm::rotate(camera_forward, rotation.y * deltaTime, right_direction);

	glm::vec3 deltaMove = movement * deltaTime;

	camera_location += (deltaMove.x * camera_forward);
	camera_location += (deltaMove.y * right_direction);
	camera_location += (deltaMove.z * camera_up);
}
