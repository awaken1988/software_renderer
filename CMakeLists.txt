cmake_minimum_required(VERSION 3.15)
project(render)


if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
endif(MSVC)

include_directories(render/include)
include_directories(render/include)
include_directories(extern/eigen3)


#extern SDL library
include_directories(extern/SDL2/include)
#add_library(SDL2 STATIC IMPORTED)
#set_target_properties(SDL2 PROPERTIES IMPORTED_LOCATION "./extern/SDL2-2.0.10/lib")

#render library
add_library(render STATIC render/render.cpp render/render_threading.cpp)	
target_compile_definitions(render PRIVATE RENDER_EXPORT)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#Test Cube
add_executable(demo_cube demo/cube/main.cpp)
target_link_libraries(demo_cube PUBLIC render)
target_link_options(demo_cube PUBLIC SDL2.lib SDL2main.lib OpenGL32.lib /LIBPATH:${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64)
target_link_options(demo_cube PUBLIC /LIBPATH:${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
file(COPY ${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64/SDL2.dll DESTINATION ${CMAKE_BINARY_DIR}/Debug/)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
file(COPY ${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64/SDL2.dll DESTINATION ${CMAKE_BINARY_DIR}/Release)