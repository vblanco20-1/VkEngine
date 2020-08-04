#pragma once

#include "glm/mat4x4.hpp"
#include <unordered_map>
#include <string>


class CameraSaver {
public:

	struct EditorUiResult {
		bool bWantsCamChange;
		std::string selectedCamera;
		glm::mat4 location;
	};

	bool load_from_file(const char* path);
	void save_to_file(const char* path);

	bool get_camera_location(std::string location, glm::mat4& outMatrix);

	void save_camera_location(std::string locationName, glm::mat4 cameraLocation);

	EditorUiResult draw_editor_ui(const glm::mat4 &cameraLocation);
private:
	std::unordered_map<std::string, glm::mat4> cameraLocations;
};