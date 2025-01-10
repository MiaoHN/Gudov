if(NOT GTEST_FOUND)
    include(FetchContent)
    FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_log test_log.cpp)
add_dependencies(test_log gudov)
force_redefine_file_macro_for_sources(test_log)
target_link_libraries(test_log gudov gtest gtest_main)
add_test(NAME test_log COMMAND test_log)

add_executable(test_config test_config.cpp)
add_dependencies(test_config gudov)
force_redefine_file_macro_for_sources(test_config)
target_link_libraries(test_config gudov gtest gtest_main)
add_test(NAME test_config COMMAND test_config)

add_executable(test_thread test_thread.cpp)
add_dependencies(test_thread gudov)
force_redefine_file_macro_for_sources(test_thread)
target_link_libraries(test_thread gudov gtest gtest_main)
add_test(NAME test_thread COMMAND test_thread)

add_executable(test_util test_util.cpp)
add_dependencies(test_util gudov)
force_redefine_file_macro_for_sources(test_util)
target_link_libraries(test_util gudov gtest gtest_main)
add_test(NAME test_util COMMAND test_util)

add_executable(test_scheduler test_scheduler.cpp)
add_dependencies(test_scheduler gudov)
force_redefine_file_macro_for_sources(test_scheduler)
target_link_libraries(test_scheduler gudov gtest gtest_main)
add_test(NAME test_scheduler COMMAND test_scheduler)

add_executable(test_fiber test_fiber.cpp)
add_dependencies(test_fiber gudov)
force_redefine_file_macro_for_sources(test_fiber)
target_link_libraries(test_fiber gudov gtest gtest_main)
add_test(NAME test_fiber COMMAND test_fiber)

add_executable(test_timer test_timer.cpp)
add_dependencies(test_timer gudov)
force_redefine_file_macro_for_sources(test_timer)
target_link_libraries(test_timer gudov gtest gtest_main)
add_test(NAME test_timer COMMAND test_timer)

add_executable(test_iomanager test_iomanager.cpp)
add_dependencies(test_iomanager gudov)
force_redefine_file_macro_for_sources(test_iomanager)
target_link_libraries(test_iomanager gudov gtest gtest_main)
add_test(NAME test_iomanager COMMAND test_iomanager)

add_executable(test_hook test_hook.cpp)
add_dependencies(test_hook gudov)
force_redefine_file_macro_for_sources(test_hook)
target_link_libraries(test_hook gudov gtest gtest_main)
add_test(NAME test_hook COMMAND test_hook)

add_executable(test_address test_address.cpp)
add_dependencies(test_address gudov)
force_redefine_file_macro_for_sources(test_address)
target_link_libraries(test_address gudov gtest gtest_main)
add_test(NAME test_address COMMAND test_address)

add_executable(test_socket test_socket.cpp)
add_dependencies(test_socket gudov)
force_redefine_file_macro_for_sources(test_socket)
target_link_libraries(test_socket gudov gtest gtest_main)
add_test(NAME test_socket COMMAND test_socket)

add_executable(test_bytearray test_bytearray.cpp)
add_dependencies(test_bytearray gudov)
force_redefine_file_macro_for_sources(test_bytearray)
target_link_libraries(test_bytearray gudov gtest gtest_main)
add_test(NAME test_bytearray COMMAND test_bytearray)

add_executable(test_http test_http.cpp)
add_dependencies(test_http gudov)
force_redefine_file_macro_for_sources(test_http)
target_link_libraries(test_http gudov gtest gtest_main)
add_test(NAME test_http COMMAND test_http)

add_executable(test_http_parser test_http_parser.cpp)
add_dependencies(test_http_parser gudov)
force_redefine_file_macro_for_sources(test_http_parser)
target_link_libraries(test_http_parser gudov gtest gtest_main)
add_test(NAME test_http_parser COMMAND test_http_parser)

add_executable(test_http_connection test_http_connection.cpp)
add_dependencies(test_http_connection gudov)
force_redefine_file_macro_for_sources(test_http_connection)
target_link_libraries(test_http_connection gudov gtest gtest_main)
add_test(NAME test_http_connection COMMAND test_http_connection)

add_executable(test_uri test_uri.cpp)
add_dependencies(test_uri gudov)
force_redefine_file_macro_for_sources(test_uri)
target_link_libraries(test_uri gudov)
add_test(NAME test_uri COMMAND test_uri)

add_executable(test_http_server test_http_server.cpp)
add_dependencies(test_http_server gudov)
force_redefine_file_macro_for_sources(test_http_server)
target_link_libraries(test_http_server gudov gtest gtest_main)