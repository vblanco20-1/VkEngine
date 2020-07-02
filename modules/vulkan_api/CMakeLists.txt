# CMakeList.txt : CMake project for decs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.13)
find_package(Vulkan REQUIRED)


file(GLOB_RECURSE PROJ_HEADERS *.h)
file(GLOB_RECURSE PROJ_SOURCES *.cpp)


set(SOURCE_FILES ${PROJ_SOURCES} ${PROJ_HEADERS})

  
 
add_library(vulkan_api ${SOURCE_FILES})
target_include_directories(vulkan_api 
		PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private"  "${CMAKE_CURRENT_SOURCE_DIR}"
		PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")



		
target_precompile_headers(vulkan_api PRIVATE pch.h)

target_link_libraries(vulkan_api PUBLIC Vulkan::Vulkan)
#target_link_libraries(vulkan_api PRIVATE  assimp  glm gli sqlite::sqlite json::json)

set_target_properties(vulkan_api PROPERTIES
            CXX_STANDARD 20
            ) 

