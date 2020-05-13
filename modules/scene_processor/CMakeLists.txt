# CMakeList.txt : CMake project for decs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.13)
find_package(Vulkan REQUIRED)

add_compile_options("/await")

add_library(scene_processor "${CMAKE_CURRENT_SOURCE_DIR}/private/scene_processor.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/public/scene_processor.h")
target_include_directories(scene_processor 
		PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private"
		PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")


target_link_libraries(scene_processor PUBLIC cppcoro::cppcoro)
target_link_libraries(scene_processor PRIVATE Vulkan::Vulkan assimp  glm gli sqlite::sqlite json::json)

set_target_properties(scene_processor PROPERTIES
            CXX_STANDARD 20
            ) 

