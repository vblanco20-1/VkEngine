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


struct InputComponent {
	glm::vec3 movement_input;
	glm::vec2 rotation_input;
};

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
	
	auto player_entity = main_registry.create();
	main_registry.assign<InputComponent>(player_entity);
	//While application is running
	bool bDontRender = false;

	int last_x = 0;
	int last_y = 0;
	bool mouselock = false;
	float cameraspeed = 1000;
	while (!quit)
	{
		main_registry.get<InputComponent>(player_entity).rotation_input = glm::vec2(0.f);
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
				//If mouse event happened
				if (e.type == SDL_MOUSEMOTION && mouselock)
				{
					//Get mouse position
					int x, y;
					SDL_GetMouseState(&x, &y);

					int deltax = last_x - x;
					int deltay = last_y - y;

					float rotationmul = .100f;

					main_registry.get<InputComponent>(player_entity).rotation_input.x += deltax *rotationmul;
					main_registry.get<InputComponent>(player_entity).rotation_input.y -= deltay *rotationmul;

					last_x = 500;
					last_y = 500;
				}
				

				//If a key was pressed
				if (e.type == SDL_KEYDOWN)
				{
					//Adjust the velocity
					switch (e.key.keysym.sym)
					{
					case SDLK_UP:
					case SDLK_w:
						main_registry.get<InputComponent>(player_entity).movement_input.x = 1;
						break;
					case SDLK_DOWN:
					case SDLK_s:
						main_registry.get<InputComponent>(player_entity).movement_input.x = -1;
						break;
					case SDLK_LEFT:
					case SDLK_a:
						main_registry.get<InputComponent>(player_entity).movement_input.y = 1;
						break;
					case SDLK_RIGHT:
					case SDLK_d:
						main_registry.get<InputComponent>(player_entity).movement_input.y = -1;
						break;
					case SDLK_SPACE:
						{
							//SDL_SetWindowGrab(sdl_get_window(),(SDL_bool)mouselock);
							mouselock = !mouselock;

						}
						break;
					}
				}
				else if (e.type == SDL_KEYUP)
				{
					//Adjust the velocity
					switch (e.key.keysym.sym)
					{
					case SDLK_UP:
					case SDLK_DOWN:		
					case SDLK_w:
					case SDLK_s:
						main_registry.get<InputComponent>(player_entity).movement_input.x = 0;
						break;
					case SDLK_LEFT:
					case SDLK_RIGHT:
					case SDLK_a:
					case SDLK_d:
						main_registry.get<InputComponent>(player_entity).movement_input.y = 0;
						break;
					}
				}
			}

			
		}

		if (mouselock) {
			SDL_WarpMouseInWindow(sdl_get_window(),500,500);
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


		InputComponent input = main_registry.get<InputComponent>(player_entity);
		glm::vec3 movementInput = glm::vec3{ input.movement_input.x, input.movement_input.y,0.f } *cameraspeed;

		vkGlobals.playerCam.update(movementInput, input.rotation_input,1.f/60.f);

		if (movementInput.x != 0)
		{
			std::cout << movementInput.x;
		}
		if (!bDontRender) {
			

			vkGlobals.draw_frame();
		}
		
	}
	_CrtDumpMemoryLeaks();
	vkGlobals.clear_vulkan();
	destroy_sdl();
	
	
	return 0;
}