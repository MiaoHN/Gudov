file(GLOB LIB_SRC
  "*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})
target_link_libraries(${PROJECT_NAME} ${YAMLCPP})
