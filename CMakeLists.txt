cmake_minimum_required(VERSION 3.16)

project(gudov)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")
include(cmake/utils.cmake)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

find_library(YAMLCPP yaml-cpp)

find_package(Boost REQUIRED)
if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

link_directories(${PROJECT_SOURCE_DIR}/lib)

include_directories(.)

add_subdirectory(gudov)
enable_testing()
add_subdirectory(test)
add_subdirectory(example)
