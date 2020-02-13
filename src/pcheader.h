#pragma once

//disable conversion related annoying warnings
#pragma warning( disable : 4018 4244 4267 4101 4838 4305)
#define WIN32_LEAN_AND_MEAN
#include <vk_mem_alloc.h>  //vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>
#undef VULKAN_HPP_NAMESPACE
#include <vulkan_enums.hpp>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL_vulkan.h>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <limits>
#include <entt/entt.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <chrono>

#define TRACY_ENABLE
#include <Tracy.hpp>
#include <TracyVulkan.hpp>
