ragelmaker(http/http11_parser.rl LIB_RAGEL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/http)
ragelmaker(http/httpclient_parser.rl LIB_RAGEL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/http)
ragelmaker(uri.rl LIB_RAGEL_SRC ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB LIB_SRC
  "http/*.cpp"
  "*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${LIB_SRC} ${LIB_RAGEL_SRC})
force_redefine_file_macro_for_sources(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} ${YAMLCPP} pthread dl)
