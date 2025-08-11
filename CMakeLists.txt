cmake_minimum_required(VERSION 3.10)
project(TaskSchedulingLibrary)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# ================================
# SOURCE FILES
# ================================
file(GLOB SOURCES "src/*.cpp")

# Create the static library
add_library(task_scheduler STATIC ${SOURCES})


# ================================
# CODE COVERAGE
# ================================
# Option for enabling code coverage
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
  include(${CMAKE_CURRENT_SOURCE_DIR}/CodeCoverage.cmake)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Code coverage enabled")
    target_compile_options(task_scheduler PRIVATE -O0 -g --coverage)
    target_link_libraries(task_scheduler PRIVATE --coverage)
  else()
    message(WARNING "Code coverage is only supported with GCC or Clang")
  endif()
endif()

# ================================
# EXAMPLES
# ================================
# Add example files directly if no separate CMakeLists.txt for examples
file(GLOB_RECURSE EXAMPLE_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} examples/*/*.cpp)
set(EXAMPLE_TARGETS "")

foreach(example_file ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${example_file} NAME_WE)
    get_filename_component(example_dir ${example_file} DIRECTORY)

    add_executable(${example_name} ${example_file})
    target_link_libraries(${example_name} task_scheduler)  # Link task_scheduler to example
    set_target_properties(${example_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${example_dir}
    )

    list(APPEND EXAMPLE_TARGETS ${example_name})
endforeach()

add_custom_target(examples DEPENDS ${EXAMPLE_TARGETS})

add_custom_target(run_all_samples
    COMMAND ${CMAKE_COMMAND} -E echo "Running all compiled examples..."
)

foreach(example_target ${EXAMPLE_TARGETS})
    add_custom_command(TARGET run_all_samples POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Running ${example_target}..."
        COMMAND ${CMAKE_SOURCE_DIR}/examples/${example_target}/${example_target}
    )
endforeach()

# ================================
# CLANG FORMATTER
# ================================
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE FORMAT_FILES
        ${CMAKE_SOURCE_DIR}/examples/*.cpp
        ${CMAKE_SOURCE_DIR}/include/*.h
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
    )
    add_custom_target(clang_format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_FILES}
        COMMENT "Running clang-format on examples/, include/, src/, and tests/"
    )
else()
    message(WARNING "clang-format not found. 'clang_format' target will be unavailable.")
endif()

# ================================
# UNIT TESTS
# ================================
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    file(GLOB TEST_SOURCES tests/*.cpp)

    add_executable(task_scheduler_tests ${TEST_SOURCES})
    target_include_directories(task_scheduler_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(task_scheduler_tests GTest::gtest GTest::gtest_main pthread task_scheduler)

    add_test(NAME task_scheduler_tests COMMAND task_scheduler_tests)

    add_custom_target(tests DEPENDS task_scheduler_tests)

    add_custom_target(run_tests
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/task_scheduler_tests --gtest_color=yes --gtest_output=verbose
        DEPENDS task_scheduler_tests
        COMMENT "Running all unit tests directly"
    )
endif()

# ================================
# EXAMPLES TARGET
# ================================
add_custom_target(examples_target
    COMMAND cmake --build ${CMAKE_BINARY_DIR} --target examples
    COMMENT "Building all examples"
)

# Run all samples target
add_custom_target(run_all_samples_target
    COMMAND cmake --build ${CMAKE_BINARY_DIR} --target run_all_samples
    COMMENT "Running all examples"
)


# ================================
# DOXYGEN DOCUMENTATION TARGET WITH doxygen-awesome-css
# ================================
find_package(Doxygen)
include(FetchContent)
FetchContent_Declare(
    doxygen-awesome-css
    URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(doxygen-awesome-css)
FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

if(DOXYGEN_FOUND)
    add_custom_target(doxyget
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen and doxygen-awesome-css"
        VERBATIM
    )
else()
    message(WARNING "Doxygen not found. Documentation target 'doxyget' will not be available.")
endif()

