﻿# CMakeList.txt : CMake project for decs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.13)

find_package(Vulkan REQUIRED)
add_library(shader_processor  "${CMAKE_CURRENT_SOURCE_DIR}/private/shader_processor.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/public/shader_processor.h")
target_include_directories(shader_processor 
		PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private"
		PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")


set_target_properties(shader_processor PROPERTIES
            CXX_STANDARD 17
            ) 

target_link_libraries(shader_processor PUBLIC cppcoro::cppcoro Vulkan::Vulkan)
target_link_libraries(shader_processor PRIVATE glslang SPIRV spirv-cross-core spirv-cross-reflect spirv-cross-cpp)

