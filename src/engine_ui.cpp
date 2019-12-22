
#include "vulkan_render.h"
#include "imgui.h"
#include "engine_ui.h"
#include "misc/cpp/imgui_stdlib.h"



bool findStringIC(const std::string& strHaystack, const std::string& strNeedle)
{
	auto it = std::search(
		strHaystack.begin(), strHaystack.end(),
		strNeedle.begin(), strNeedle.end(),
		[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
	);
	return (it != strHaystack.end());
}


void UI::DrawEngineUI(VulkanEngine* engine)
{
	ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);

	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
	static bool p_open = true;
	ImGui::Begin("Example: Simple overlay", &p_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);


	ImGui::Text("frametime = %f", engine->eng_stats.frametime);
	ImGui::Text("drawcalls = %d", engine->eng_stats.drawcalls);

	ImGui::InputFloat3("Camera Up", &engine->config_parameters.CamUp[0]);
	ImGui::InputFloat("Camera FOV", &engine->config_parameters.fov);

	ImGui::Separator();

	static std::string search_string;

	ImGui::InputText("Search", &search_string);
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 pos = ImGui::GetCursorScreenPos();

	auto found = [](const std::string &str)->bool {
		if (search_string.length() == 0) {
			return true;
		}
		else {
			return findStringIC(str,search_string);
		}
	};

	if(ImGui::TreeNode("Resources"))
	{
		if (ImGui::TreeNode("Textures"))
		{
			//ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::BeginChild("Textures", ImVec2(ImGui::GetWindowContentRegionWidth()*0.9,400), false);//,// window_flags);
			
			
		
			for (auto [K, V] : engine->resourceMap) {
		
				if (found(K)&&  engine->render_registry.has<TextureResource>(V)) {
					ImGui::Text(K.c_str());
		
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
		
						float my_tex_w = 512;//(float)io.Fonts->TexWidth;
						float my_tex_h = 512;//(float)io.Fonts->TexHeight;
						auto texsize = engine->render_registry.get<TextureResourceMetadata>(V).texture_size;
		
						float region_sz = 32.0f;
						float region_x = io.MousePos.x - pos.x - region_sz * 0.5f; if (region_x < 0.0f) region_x = 0.0f; else if (region_x > my_tex_w - region_sz) region_x = my_tex_w - region_sz;
						float region_y = io.MousePos.y - pos.y - region_sz * 0.5f; if (region_y < 0.0f) region_y = 0.0f; else if (region_y > my_tex_h - region_sz) region_y = my_tex_h - region_sz;
						float zoom = 4.0f;
						ImGui::Text("Size: (%i, %i)", texsize.x, texsize.y);
						
						ImVec2 uv0 = { 0,0 };//ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
						ImVec2 uv1 = { 1,1};//ImVec2((region_x + region_sz) / my_tex_w, (region_y + region_sz) / my_tex_h);
		
						ImTextureID image = (ImTextureID)(intptr_t)engine->render_registry.get<TextureResource>(V).image.image.operator VkImage();
		
						ImGui::Image(image, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
						ImGui::EndTooltip();
					}
		
				}
			}
		
			ImGui::EndChild();
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Meshes"))
		{
			ImGui::BeginChild("Textures", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.9, 400), false);
			ImGui::Separator();
		
			engine->resourceMap;
		
			for (auto [K, V] : engine->resourceMap) {
		
				if (found(K) && engine->render_registry.has<MeshResource>(V)) {
					ImGui::Text(K.c_str());					
				}
			}
			ImGui::EndChild();
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Pipelines"))
		{
			ImGui::BeginChild("Textures", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.9, 400), false);
			ImGui::Separator();
		
			engine->resourceMap;
		
			for (auto [K, V] : engine->resourceMap) {
		
				if (found(K) && engine->render_registry.has<PipelineResource>(V)) {
					ImGui::Text(K.c_str());
				}
			}
			ImGui::EndChild();
			ImGui::TreePop();
		}
		
		
		
		ImGui::Separator();
		ImGui::TreePop();
	}

	ImGui::End();
}

