cmake_minimum_required (VERSION 3.13)

include(FetchContent)
include(ExternalProject)
set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(imgui STATIC)
add_library(entt INTERFACE)
add_library(glm INTERFACE)
add_library(gli INTERFACE)
add_library(assimp INTERFACE)
add_library(asimp INTERFACE)
add_library(sdl2 INTERFACE)
add_library(vma INTERFACE)
add_library(microguid INTERFACE)


#add_library(glslang INTERFACE)
#add_library(spirv INTERFACE)

find_package(Vulkan REQUIRED) 

add_subdirectory(meshoptimizer)
add_subdirectory(nv_aftermath)
add_subdirectory(json)
add_subdirectory(sqlite)
add_subdirectory(cppcoro)
#SDL ------------------------------------------

set(sdl2_DIR "K:/Programming/SDL2-2.0.8" CACHE FILEPATH "Path to SDL2")
set(sdl2_image_DIR "K:/Programming/SDL2_image-2.0.4" CACHE FILEPATH "Path to SDL2)image")

target_include_directories(sdl2 INTERFACE "${sdl2_DIR}/include" "${sdl2_image_DIR}/include" )
target_link_directories(sdl2 INTERFACE "${sdl2_DIR}/lib/x64" "${sdl2_image_DIR}/lib/x64")
target_link_libraries(sdl2 INTERFACE SDL2_image SDL2 SDL2main)

#ENTT -----------------------------------
FetchContent_Declare(ext_entt
		  GIT_REPOSITORY    https://github.com/skypjack/entt
		  GIT_TAG           master
		  GIT_SHALLOW 1
		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/entt"
		  CONFIGURE_COMMAND ""
		  BUILD_COMMAND     ""
		  INSTALL_COMMAND   ""
		  TEST_COMMAND      ""
)

FetchContent_GetProperties(ext_entt)
if(NOT ext_entt_POPULATED) 
   FetchContent_Populate(ext_entt)
   target_include_directories(entt INTERFACE "${ext_entt_SOURCE_DIR}/src")   
endif()


message(STATUS "entt directory: ${ext_entt_SOURCE_DIR}")

#microguid -----------------------------------

target_include_directories(microguid INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/microGUID")  

#IMGUI -------------------------------------


FetchContent_Declare(ext_imgui
		  GIT_REPOSITORY    https://github.com/ocornut/imgui
		  GIT_TAG           master
		  GIT_SHALLOW 1
		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
		  CONFIGURE_COMMAND ""
		  BUILD_COMMAND     ""
		  INSTALL_COMMAND   ""
		  TEST_COMMAND      ""
)
FetchContent_GetProperties(ext_imgui)
if(NOT ext_imgui_POPULATED) 
   FetchContent_Populate(ext_imgui)
   target_include_directories(imgui PUBLIC ${ext_imgui_SOURCE_DIR})
   target_include_directories(imgui PUBLIC "${ext_imgui_SOURCE_DIR}/examples")
   target_include_directories(imgui PUBLIC "${ext_imgui_SOURCE_DIR}/misc/cpp")
   set(imgui_sources 
   "${ext_imgui_SOURCE_DIR}/imgui.cpp"
   "${ext_imgui_SOURCE_DIR}/imgui_demo.cpp"
   "${ext_imgui_SOURCE_DIR}/imgui_draw.cpp"   
   "${ext_imgui_SOURCE_DIR}/imgui_widgets.cpp"
   "${ext_imgui_SOURCE_DIR}/examples/imgui_impl_vulkan.cpp"
   "${ext_imgui_SOURCE_DIR}/examples/imgui_impl_sdl.cpp"
   "${ext_imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
   )
   target_sources(imgui PRIVATE ${imgui_sources})
endif()

target_link_libraries(imgui sdl2 Vulkan::Vulkan)
message(STATUS "imgui directory: ${ext_imgui_SOURCE_DIR}")


#GLM -----------------------------------
FetchContent_Declare(ext_glm
		  GIT_REPOSITORY    https://github.com/g-truc/glm
		  GIT_TAG           master
		  GIT_SHALLOW 1
		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/glm"
		  CONFIGURE_COMMAND ""
		  BUILD_COMMAND     ""
		  INSTALL_COMMAND   ""
		  TEST_COMMAND      ""
)

FetchContent_GetProperties(ext_glm)
if(NOT ext_glm_POPULATED) 
   FetchContent_Populate(ext_glm)
   #target_include_directories(glm INTERFACE "${ext_glm_SOURCE_DIR}/glm")   
endif()
target_include_directories(glm INTERFACE "${ext_glm_SOURCE_DIR}")

message(STATUS "glm directory: ${ext_glm_SOURCE_DIR}")

#GLI -----------------------------------
FetchContent_Declare(ext_gli
		  GIT_REPOSITORY    https://github.com/g-truc/gli
		  GIT_TAG           master
		  GIT_SHALLOW 1
		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/gli"
		  CONFIGURE_COMMAND ""
		  BUILD_COMMAND     ""
		  INSTALL_COMMAND   ""
		  TEST_COMMAND      ""
)

FetchContent_GetProperties(ext_gli)
if(NOT ext_gli_POPULATED) 
   FetchContent_Populate(ext_gli)   
endif()

target_include_directories(gli INTERFACE "${ext_gli_SOURCE_DIR}")

#VMA -----------------------------------
FetchContent_Declare(ext_vma
		  GIT_REPOSITORY    https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
		  GIT_TAG           master
		  GIT_SHALLOW 1
		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/vma"
		  CONFIGURE_COMMAND ""
		  BUILD_COMMAND     ""
		  INSTALL_COMMAND   ""
		  TEST_COMMAND      ""
)

FetchContent_GetProperties(ext_vma)
if(NOT ext_vma_POPULATED) 
   FetchContent_Populate(ext_vma)   
endif()

target_include_directories(vma INTERFACE "${ext_vma_SOURCE_DIR}/src")



#GLSLANG -----------------------------------
ExternalProject_Add(ext_glslang
		  GIT_REPOSITORY    https://github.com/KhronosGroup/glslang
		  GIT_TAG           master
		  GIT_SHALLOW 1
		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/glslang"
		  BINARY_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/glslang_build"
		  CONFIGURE_COMMAND ""
		  BUILD_COMMAND    ""
)

add_subdirectory(glslang )

#SPIRV -----------------------------------
ExternalProject_Add(ext_spirv
		  GIT_REPOSITORY    https://github.com/KhronosGroup/SPIRV-Cross
		  GIT_TAG           master
		  GIT_SHALLOW 1
		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross"
		  BINARY_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_build"
		  CONFIGURE_COMMAND ""
		  BUILD_COMMAND     ""
		  #BUILD_COMMAND    ${CMAKE_COMMAND} -E echo "Starting $<CONFIG> build"
		  #COMMAND          ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR>
		  #COMMAND      ${CMAKE_COMMAND}    -E echo "spirv cmake projects generated"
		  #COMMAND       ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
		  INSTALL_COMMAND   ""
		  TEST_COMMAND      ""
)


add_subdirectory(spirv_cross )



set(TRACY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tracy" CACHE FILEPATH "Path to Tracy")
add_library(tracy OBJECT "${TRACY_DIR}/TracyClient.cpp")
target_include_directories(tracy PUBLIC "${TRACY_DIR}")
#target_include_directories(spirv INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross")

#file(GLOB_RECURSE spirv_libs
#	"${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_build/Release/*.lib"  
#    )

#target_link_libraries(spirv INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_build/Release/spirv-cross-c.lib" )

#target_link_libraries(spirv INTERFACE "${spirv_libs}" )
#set(glslang_include "${CMAKE_CURRENT_SOURCE_DIR}/glslang")
#
#add_dependencies(glslang ext_glslang)
#target_include_directories(glslang INTERFACE "${glslang_include}")


#ASSIMP -----------------------------------
#ExternalProject_Add(ext_assimp
#		  GIT_REPOSITORY    https://github.com/assimp/assimp
#		  GIT_TAG           master
#		  GIT_SHALLOW 1
#		  SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/assimp"
#		  BINARY_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/assimp_build"
#		  CONFIGURE_COMMAND ""
#		  BUILD_COMMAND    ${CMAKE_COMMAND} -E echo "Starting $<CONFIG> build"
#		  COMMAND          ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR>
#		  COMMAND      ${CMAKE_COMMAND}    -E echo "assimp cmake projects generated"
#		  COMMAND       ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
#		  INSTALL_COMMAND   ""
#		  TEST_COMMAND      ""
#)

set(assimp_dir "${CMAKE_CURRENT_SOURCE_DIR}/assimp_build")
set(assimp_dll "${CMAKE_CURRENT_SOURCE_DIR}/assimp_build/code/Release/assimp-vc142-mt.dll")
set(assimp_lib "${CMAKE_CURRENT_SOURCE_DIR}/assimp_build/code/Release/assimp-vc142-mt.lib")
#add_dependencies(assimp ext_asimp)

target_include_directories(assimp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include")
target_link_libraries(assimp INTERFACE "${assimp_lib}" )

target_include_directories(asimp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include")
target_link_libraries(asimp INTERFACE "${assimp_lib}" )


