cmake_minimum_required(VERSION 3.10)

project(Assignment_2_OpenGL)

set(VERTEX_SHADERS 
	# add vertex shader (.glsl) files here
	Shaders/VertexShader.glsl
)

set(PIXEL_SHADERS 
	# add pixel shader (.glsl) files here
	Shaders/FragmentShader.glsl
)

# Add any new C/C++ source code here
set(SOURCE_CODE
	# Header & CPP files go here
	main.cpp
	renderer.h
	OpenGLExtensions.h
	FileIntoString.h
	h2bParser.h
	load_object_oriented.h
)

if(WIN32)
# by default CMake selects "ALL_BUILD" as the startup project
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
		 PROPERTY VS_STARTUP_PROJECT Assignment_2_OpenGL)

	#this will tell Visual Studio to put the shaders under their own group in the solution explorer 
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
   	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VERTEX_SHADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PIXEL_SHADERS})
endif()

# currently using unicode in some libraries on win32 but will change soon
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

if (UNIX AND NOT APPLE)
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lX11 -lGL")
     find_package(X11)
     find_package(OpenGL)
     include_directories(${X11_INCLUDE_DIR})
     link_libraries(${X11_LIBRARIES})
     include_directories(${OPENGL_INCLUDE_DIRS})
     link_libraries(${OPENGL_LIBRARIES})
endif()

add_executable (Assignment_2_OpenGL 
	${SOURCE_CODE}
	${VERTEX_SHADERS}
	${PIXEL_SHADERS}
)


