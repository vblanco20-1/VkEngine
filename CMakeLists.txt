# CMakeList.txt : CMake project for decs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.13)
project(vkEngine VERSION 0.1.0)

add_subdirectory(third_party)
add_subdirectory(modules)
add_subdirectory(src)

add_compile_options("/await")

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.frag"
     "${PROJECT_SOURCE_DIR}/assets/shaders/*.comp"
    "${PROJECT_SOURCE_DIR}/assets/shaders/*.vert"
	"${PROJECT_SOURCE_DIR}/assets/shaders/*.inl"
    )

source_group("Shaders" FILES ${GLSL_SOURCE_FILES})
add_library(
    Shaders 
	"${PROJECT_SOURCE_DIR}/assets/shaders/dummy.cpp"
${GLSL_SOURCE_FILES}
    )


#source_group(Shaders ${GLSL_SOURCE_FILES}) 