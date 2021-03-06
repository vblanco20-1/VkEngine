#pragma once

//disable conversion related annoying warnings
#pragma warning( disable : 4018 4244 4267 4101 4838 4305)
#define WIN32_LEAN_AND_MEAN
#define VMA_BUFFER_DEVICE_ADDRESS 1
#include <vk_mem_alloc.h>  //vk_mem_alloc.h"

#define VK_ENABLE_BETA_EXTENSIONS

#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan.hpp>
#undef VULKAN_HPP_NAMESPACE
//#include <vulkan_enums.hpp>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <gli/gli.hpp>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_vulkan.h>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <limits>
#include <future>
#include <thread>
#include <unordered_map>
#include <vector>
#include <entt/entt.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include <microGUID.hpp>
#include "NsightAftermathGpuCrashTracker.h"
#include "termcolor.hpp"
#include <chrono>

#define TRACY_ENABLE
#include <Tracy.hpp>
#include <TracyVulkan.hpp>
