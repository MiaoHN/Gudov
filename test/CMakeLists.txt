enable_testing()

add_executable(test log_test.cpp)
add_dependencies(test gudov)
force_redefine_file_macro_for_sources(test)
target_link_libraries(test gudov)
add_test(NAME test COMMAND test)

add_executable(config_test config_test.cpp)
add_dependencies(config_test gudov)
force_redefine_file_macro_for_sources(config_test)
target_link_libraries(config_test gudov)
add_test(NAME config_test COMMAND config_test)

add_executable(thread_test thread_test.cpp)
add_dependencies(thread_test gudov)
force_redefine_file_macro_for_sources(thread_test)
target_link_libraries(thread_test gudov)
add_test(NAME thread_test COMMAND thread_test)
