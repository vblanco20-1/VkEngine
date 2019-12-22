cmake_minimum_required (VERSION 3.13)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Vulkan REQUIRED)

add_compile_definitions(ASSET_PATH="${PROJECT_SOURCE_DIR}/assets/")

set(SOURCE_FILES main.cpp  sdl_render.cpp vmath.cpp vulkan_render.cpp pcheader.cpp vulkan_textures.cpp engine_ui.cpp vulkan_init.cpp vulkan_descriptors.cpp)


add_executable(vkEngine ${SOURCE_FILES})



target_link_libraries(vkEngine entt imgui glm gli assimp sdl2 vma Vulkan::Vulkan)

set_target_properties(vkEngine PROPERTIES
            CXX_STANDARD 17
            )