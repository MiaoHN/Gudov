cmake_minimum_required(VERSION 3.20)

project(Gudov)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(${PROJECT_NAME} gudov/main.cpp)