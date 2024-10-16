cmake_minimum_required(VERSION 3.10)

project(Lemmler2024 C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(glfw3 3.4 REQUIRED)

find_package(glm REQUIRED)
# find_package(GLEW REQUIRED)
# include_directories(${GLM_INCLUDE_DIRS})

file(GLOB_RECURSE LEMMLER2024_SOURCES
	src/*.c
    src/*.cpp
    src/*.h
    src/*.hpp
    include/*.h
	)


add_executable(Sample ${LEMMLER2024_SOURCES})

target_link_libraries(Sample PRIVATE opengl32 glfw glm::glm)
# target_include_directories(Sample PRIVATE ./src)
target_compile_definitions(Sample PRIVATE "SCENE_DIR=\"${CMAKE_SOURCE_DIR}/mesh\"")
target_include_directories(Sample PUBLIC ./include)
