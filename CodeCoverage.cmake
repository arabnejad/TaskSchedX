# Code coverage configuration
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Only proceed with coverage configuration if it's enabled
    if(ENABLE_COVERAGE)
        message(STATUS "Code coverage is enabled")

        # Find required tools
        find_program(LCOV lcov REQUIRED)
        find_program(GENHTML genhtml REQUIRED)
        find_program(GCOVR gcovr REQUIRED)

        # Add coverage targets
        add_custom_target(coverage
            # Clean old coverage data
            COMMAND ${LCOV} --directory . --zerocounters

            # Run tests
            COMMAND ctest --output-on-failure

            # Capture coverage data
            COMMAND ${LCOV} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV} --remove coverage.info '/usr/*' '*/tests/*' '*/googletest/*' --output-file coverage.info.cleaned
            COMMAND ${GENHTML} -o coverage coverage.info.cleaned
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report"
        )

        add_custom_target(gcovr
            COMMAND ${GCOVR}
            --exclude-directories build/_deps
            --exclude-directories tests
            --exclude /Library/
            --exclude /usr/
            -r ${CMAKE_SOURCE_DIR}
            --html
            --html-details
            -o coverage.html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating HTML coverage report with gcovr"
        )

        add_custom_target(gcovr_console
            COMMAND ${GCOVR}
            --gcov-ignore-parse-errors=negative_hits.warn
            --exclude-directories build/_deps
            --exclude-directories tests
            --exclude /Library/
            --exclude /usr/
            -r ${CMAKE_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating console coverage report with gcovr"
        )

    endif()
endif()