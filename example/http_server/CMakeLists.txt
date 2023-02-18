set(EXECUTABLE_OUTPUT_PATH ${OUTPUT_ROOT}/http_server)

add_executable(http_server http_server.cpp)
add_dependencies(http_server gudov)
force_redefine_file_macro_for_sources(http_server)
target_link_libraries(http_server gudov)

add_custom_command(
    TARGET http_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/conf
            $<TARGET_FILE_DIR:http_server>/conf
            # ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(
    TARGET http_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/html
            $<TARGET_FILE_DIR:http_server>/html
            # ${CMAKE_CURRENT_BINARY_DIR}
)