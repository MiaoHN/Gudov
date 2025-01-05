cmake_minimum_required(VERSION 3.16)

project(gudov)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -ggdb -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")
include(cmake/utils.cmake)

set(OUTPUT_ROOT ${PROJECT_SOURCE_DIR}/bin)

set(EXECUTABLE_OUTPUT_PATH ${OUTPUT_ROOT})
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

find_library(YAMLCPP yaml-cpp)

find_package(Boost REQUIRED)
if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

link_directories(${PROJECT_SOURCE_DIR}/lib)

include_directories(.)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DGUDOV_DEBUG)
endif()

add_subdirectory(gudov)
enable_testing()
add_subdirectory(test)
add_subdirectory(example)

# move `conf` to bin

add_custom_target(move_conf ALL
    COMMAND cp -r ${PROJECT_SOURCE_DIR}/conf ${OUTPUT_ROOT}
    COMMENT "move conf to bin"
    VERBATIM
)