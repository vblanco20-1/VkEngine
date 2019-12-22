# CMakeList.txt : CMake project for decs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.13)
project(vkEngine VERSION 0.1.0)

add_subdirectory(third_party)
add_subdirectory(src)

#find_package(Vulkan REQUIRED)
#
#set(SOURCE_FILES main.cpp  sdl_render.cpp vmath.cpp vulkan_render.cpp pcheader.cpp vulkan_textures.cpp engine_ui.cpp vulkan_init.cpp vulkan_descriptors.cpp)
#
#
#add_executable(vkEngine "src/main.cpp")
#target_link_libraries(vkEngine entt imgui glm gli assimp sdl2 vma Vulkan::Vulkan)
#
#set_target_properties(vkEngine PROPERTIES
#            CXX_STANDARD 17
#            )
			
#add_custom_command(TARGET vkEngine PRE_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        "${assimp_dll}"
#        $<TARGET_FILE_DIR:vkEngine>)

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()


file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.vert"
    )
message(STATUS "TEST")
message(STATUS "${PROJECT_SOURCE_DIR}/assets/shaders/*.frag")
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    #COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/assets/shaders/"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
