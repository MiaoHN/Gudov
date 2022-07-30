cmake_minimum_required(VERSION 3.20)

project(gudov)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

find_library(YAMLCPP yaml-cpp)

link_directories(${PROJECT_SOURCE_DIR}/lib)

include_directories(.)

add_subdirectory(gudov)
add_subdirectory(test)
