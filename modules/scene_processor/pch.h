#define NOMINMAX
#include <sqlite3.h>
#include <stdio.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>

#include <vulkan/vulkan.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <functional>

#include <nlohmann/json.hpp>

#include "stb_image.h"

#include <gli/gli.hpp>
#include <vk_format.h>