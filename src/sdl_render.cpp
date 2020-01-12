#include "sdl_render.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_vulkan.h>
#include "entt/entt.hpp"
//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>
//#define TINYOBJLOADER_IMPLEMENTATION
//#include <tiny_obj_loader.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
SDL_Renderer *gRenderer;
SDL_Window *gWindow;

bool initialize_sdl()
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);// | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	gWindow = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		1700,
		900,
		window_flags
	);
	assert(gWindow!= nullptr);
	//Initialize PNG loading
	int imgFlags = IMG_INIT_PNG;
	if (!(IMG_Init(imgFlags) & imgFlags))
	{
		printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
		return false;
	}

	//gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!gRenderer)
	{
		return false;
	}

	//SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

	return true;
}

void start_frame()
{
	//Clear screen
	SDL_RenderClear(gRenderer);
}

void end_frame()
{
	//Update screen
	SDL_RenderPresent(gRenderer);
}

void destroy_sdl()
{
	SDL_Delay(3000);

	SDL_DestroyWindow(gWindow);
	SDL_Quit();
}

SDL_Window* sdl_get_window()
{
	return gWindow;
}

SDL_Renderer* get_main_renderer()
{
	return gRenderer;
}
