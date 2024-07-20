cmake_minimum_required(VERSION 3.20)

# incorporate c to fix assimp bug
project(Yume LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "-std=c99")

set(EXE_NAME Yume)

file(GLOB CPP_SRC 
	"src/*.h"
	"src/*.cpp"
)

file(GLOB CORE_SRC 
	"src/core/*.h"
	"src/core/*.cpp"
)

file(GLOB CUDA_SRC
	"src/*.cu"
	"src/*.cuh"
)

file(GLOB IMGUI_SRC
	"${PROJECT_SOURCE_DIR}/external/imgui/*.h"
	"${PROJECT_SOURCE_DIR}/external/imgui/*.cpp"
)

source_group("CUDA" FILES ${CUDA_SRC})
source_group("imgui" FILES ${IMGUI_SRC})
source_group("core" FILES ${CORE_SRC})

add_executable(${EXE_NAME} ${CPP_SRC} ${CUDA_SRC} ${IMGUI_SRC} ${CORE_SRC})
set_target_properties(${EXE_NAME} PROPERTIES
					CUDA_SEPARABLE_COMPILATION ON
					CUDA_ARCHITECTURES native
					)



target_include_directories(${EXE_NAME} PRIVATE "${CMAKE_SOURCE_DIR}")
target_include_directories(${EXE_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/external/glm")
target_include_directories(${EXE_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/external/assimp-5.1.0/include")
target_include_directories(${EXE_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/external/imgui")
target_include_directories(${EXE_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/external/image_lib")
target_include_directories(${EXE_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/src")

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glad)
add_subdirectory(external/glfw)
add_subdirectory(external/assimp-5.1.0)


target_link_libraries(${EXE_NAME} PRIVATE glfw glad assimp)

add_custom_command(
	TARGET ${EXE_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${PROJECT_SOURCE_DIR}/src/shaders"
	"$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/shaders"
)

add_custom_command(
	TARGET ${EXE_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${PROJECT_SOURCE_DIR}/src/model"
	"$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/model"
)

add_custom_command(
	TARGET ${EXE_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${PROJECT_SOURCE_DIR}/src/test_assests"
	"$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/test_assests"
)