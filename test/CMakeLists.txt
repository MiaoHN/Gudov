enable_testing()

add_executable(test log_test.cpp)
add_dependencies(test gudov)
target_link_libraries(test gudov)
add_test(NAME test COMMAND test)

add_executable(config_test config_test.cpp)
add_dependencies(config_test gudov)
target_link_libraries(config_test gudov)
add_test(NAME config_test COMMAND config_test)
