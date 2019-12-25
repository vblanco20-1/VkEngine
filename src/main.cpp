#include <SDL.h>

#include "entt/entt.hpp"
#include "components.h"
#include "sdl_render.h"
#include "vmath.h"

#include "vulkan_render.h"
#include <iostream>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#include "engine_ui.h"
#include "shader_processor.h"

int main(int argc, char *argv[])
{
	//compile_shader("C:/Programming/vkEngine/assets/shaders/basiclit.vert",nullptr);
	//return 0;

	auto main_registry = entt::registry{};
	
	bool quit = false;
	SDL_Event e;

	initialize_sdl();

	VulkanEngine vkGlobals;
	//try {
		vkGlobals.init_vulkan();
	//}	
	///catch (std::runtime_error& e)
	///{
	///	std::cout << e.what() << std::endl;
	//}
	

	//While application is running
	bool bDontRender = false;
	while (!quit)
	{

		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			//User requests quit
			if (e.type == SDL_QUIT)
			{
				quit = true;
			}
			else
			{
				if (e.type == SDL_WINDOWEVENT)
				{
					switch (e.window.event) {
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					case SDL_WINDOWEVENT_RESIZED:
						vkGlobals.recreate_swapchain();
						break;

					case SDL_WINDOWEVENT_MINIMIZED:
						bDontRender = true;
						break;
					case SDL_WINDOWEVENT_RESTORED:
						bDontRender = false;
						break;
					}
					
				}
				//If a key was pressed
				if (e.type == SDL_KEYDOWN)
				{
					//Adjust the velocity
					switch (e.key.keysym.sym)
					{
					//case SDLK_UP:
					//	main_registry.get<PlayerInputComponent>(player_entity).movement_input.y = 1;
					//	break;
					//case SDLK_DOWN:
					//	main_registry.get<PlayerInputComponent>(player_entity).movement_input.y = -1;
					//	break;
					//case SDLK_LEFT:
					//	main_registry.get<PlayerInputComponent>(player_entity).movement_input.x = -1;
					//	break;
					//case SDLK_RIGHT:
					//	main_registry.get<PlayerInputComponent>(player_entity).movement_input.x = 1;
					//	break;
					}
				}
				else if (e.type == SDL_KEYUP)
				{
					//Adjust the velocity
					switch (e.key.keysym.sym)
					{
				//	case SDLK_UP:
				//		main_registry.get<PlayerInputComponent>(player_entity).movement_input.y = 0;
				//		break;
				//	case SDLK_DOWN:
				//		main_registry.get<PlayerInputComponent>(player_entity).movement_input.y = 0;
				//		break;
				//	case SDLK_LEFT:
				//		main_registry.get<PlayerInputComponent>(player_entity).movement_input.x = 0;
				//		break;
				//	case SDLK_RIGHT:
				//		main_registry.get<PlayerInputComponent>(player_entity).movement_input.x = 0;
				//		break;
					}
				}
			}
		}
		//start_frame();
		FrameMark;
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(sdl_get_window());
		ImGui::NewFrame();
		static bool bShowDemo = true;
		ImGui::ShowDemoWindow(&bShowDemo);
		
		UI::DrawEngineUI(&vkGlobals);

		ImGui::Render();

		if (!bDontRender) {

			vkGlobals.draw_frame();
		}
		
	}

	vkGlobals.clear_vulkan();
	destroy_sdl();
	
	
	return 0;
}