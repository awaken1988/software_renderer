cmake_minimum_required(VERSION 3.15)
project(render)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
endif(MSVC)


set(RENDER_H render/include/render.h render/include/render_threading.h)

function(ADD_EXE_DEP A_TARGET)
	target_link_libraries(${A_TARGET} PUBLIC render)
	target_link_options(${A_TARGET} PUBLIC SDL2.lib SDL2main.lib OpenGL32.lib /LIBPATH:${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64)
	target_link_options(${A_TARGET} PUBLIC /LIBPATH:${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64)
	target_include_directories(${A_TARGET} PRIVATE render/include/ )
endfunction()

include_directories(render/include)
include_directories(render/include)
include_directories(extern/eigen3)
include_directories(extern/SDL2/include)

#extern SDL library
add_library(render STATIC render/render.cpp render/render_threading.cpp ${RENDER_H} )
target_include_directories(render PRIVATE render/include)
target_compile_definitions(render PRIVATE RENDER_EXPORT)

#demo_cube
add_executable(demo_cube demo/cube/main.cpp ${RENDER_H} demo/sdl2_helper.h)
ADD_EXE_DEP(demo_cube)

#demo_fps
add_executable(demo_fps demo/fps/main.cpp ${RENDER_H} demo/sdl2_helper.h)
ADD_EXE_DEP(demo_fps)


file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
file(COPY ${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64/SDL2.dll DESTINATION ${CMAKE_BINARY_DIR}/Debug/)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
file(COPY ${CMAKE_SOURCE_DIR}/extern/SDL2/lib/x64/SDL2.dll DESTINATION ${CMAKE_BINARY_DIR}/Release)