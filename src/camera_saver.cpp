
#include "camera_saver.h"
#include "imgui.h"

bool CameraSaver::load_from_file(const char* path)
{
	return false;
}

void CameraSaver::save_to_file(const char* path)
{

}

bool CameraSaver::get_camera_location(std::string location, glm::mat4& outMatrix)
{
	auto it = cameraLocations.find(location);
	if (it != cameraLocations.end()) {
		outMatrix = (*it).second;
		return true;
	}
	else {
		return false;
	}
}

void CameraSaver::save_camera_location(std::string locationName, glm::mat4 cameraLocation)
{
	cameraLocations[locationName] = cameraLocation;
}

CameraSaver::EditorUiResult CameraSaver::draw_editor_ui(const glm::mat4& currentCamLocation)
{
	ImGui::Separator();

	static char *stringbuffer = []() -> char* {
	
		char* bf = new char[100];
		strcpy(bf, "dummy camera name");
		return bf;
	}();

	ImGui::InputText("Location name", stringbuffer, 100);
	if (ImGui::Button("Save Camera")) {
		std::string str = stringbuffer;
		save_camera_location(str, currentCamLocation);
	}
	ImGui::Separator();
	ImGui::Text("Saved Cameras");
	for (auto [k, v] : cameraLocations) {
		ImGui::PushID(k.c_str());
		ImGui::Text(k.c_str());
		ImGui::SameLine();

		if (ImGui::Button("Cam"))
		{
			EditorUiResult res;
			res.bWantsCamChange = true;
			res.location = cameraLocations[k];
			res.selectedCamera = k;

			ImGui::PopID();
			return res;
		}
		ImGui::PopID();
	}

	EditorUiResult res;
	res.bWantsCamChange = false;
	return res;
}
