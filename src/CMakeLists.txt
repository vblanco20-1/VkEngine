cmake_minimum_required (VERSION 3.16)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Vulkan REQUIRED)
 
file(GLOB PROJ_HEADERS *.h)
file(GLOB PROJ_SOURCES *.cpp)

add_compile_definitions(ASSET_PATH="${PROJECT_SOURCE_DIR}/assets/")

set(SOURCE_FILES ${PROJ_SOURCES} ${PROJ_HEADERS})

 
 add_compile_options("/await")
 
add_executable(vkEngine ${SOURCE_FILES}) 

 
target_precompile_headers(vkEngine PUBLIC pcheader.h)
target_link_libraries(vkEngine  imgui entt glm gli assimp sdl2 vma tracy nv_aftermath::nv_aftermath vulkan_api)
target_link_libraries(vkEngine shader_processor scene_processor)
set_target_properties(vkEngine PROPERTIES
            CXX_STANDARD 20
            ) 