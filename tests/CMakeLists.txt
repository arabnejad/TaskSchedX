# Enable testing
enable_testing()

# Add the GoogleTest subdirectory (assuming you have it installed or downloaded)
# If you are using `gtest` via a package manager or as a submodule, uncomment this line
# add_subdirectory(<path_to_googletest>)

# Add the test files
file(GLOB TEST_SOURCES "*.cpp")

# Create the test executable
add_executable(run_tests ${TEST_SOURCES})

# Link GoogleTest and the task_scheduler library
target_link_libraries(run_tests gtest gtest_main task_scheduler)

# Run tests
add_test(NAME task_scheduler_tests COMMAND run_tests)
