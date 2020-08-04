#pragma once
#include <glm/glm.hpp>


class PlayerCamera {
public:
	glm::vec3 camera_forward;
	glm::vec3 camera_up;	
	glm::vec3 camera_location;
	glm::vec3 eye;
	glm::mat4 view_matrix;
	void rebuild_matrix();
	void update(glm::vec3 movement,glm::vec2 rotation, float deltaTime);
};

