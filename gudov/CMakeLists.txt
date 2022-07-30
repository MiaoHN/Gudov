file(GLOB LIB_SRC
  "*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} ${YAMLCPP} pthread)
