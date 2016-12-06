cmake_minimum_required(VERSION 3.6)
project(CS457_P3)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES manager.cpp cost_info.cpp cost_info.h p3method.cpp p3method.h dijkstra.cpp dijkstra.h router.cpp)
add_executable(CS457_P3 ${SOURCE_FILES})