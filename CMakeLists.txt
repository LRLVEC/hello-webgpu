cmake_minimum_required(VERSION 3.20)
project(hello)
set(CMAKE_CXX_STANDARD 20)

add_executable(hello "hello.cpp")

if(EMSCRIPTEN)
	set_target_properties(hello PROPERTIES SUFFIX ".html")
	target_link_options(hello PRIVATE "-sUSE_WEBGPU=1" "-sUSE_GLFW=3")
else()
	set(DAWN_FETCH_DEPENDENCIES ON)
	add_subdirectory("dependencies/dawn" EXCLUDE_FROM_ALL)
	target_link_libraries(hello PRIVATE webgpu_cpp webgpu_dawn webgpu_glfw)
endif()