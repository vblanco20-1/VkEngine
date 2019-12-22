#pragma once

#include "entt/fwd.hpp"
#include "vmath.h"
#include "SDL_rect.h"
#include <string>

constexpr int WINDOW_WIDTH = 640;
constexpr int WINDOW_HEIGHT = 800;

constexpr Vec2i UP{ 0,-1 };
constexpr Vec2i RIGHT{ 1,0 };

constexpr Vec2i coordinate_center{ WINDOW_WIDTH / 2,WINDOW_HEIGHT - 100 };

struct SDL_Renderer;
struct SDL_Window;
struct SDL_Texture;


bool initialize_sdl();
void start_frame();
void end_frame();
void destroy_sdl();
SDL_Window* sdl_get_window();
SDL_Renderer* get_main_renderer();
//converts game space (centered) to screen space
constexpr Vec2i game_space_to_screen_space(Vec2f location) {
	auto up = (UP * location.y);
	auto right = (RIGHT * location.x);
	return coordinate_center + up + right;
}
