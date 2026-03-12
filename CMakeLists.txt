cmake_minimum_required(VERSION 3.10)
project(MiniCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(compiler_core
    src/lexer/token.cpp
    src/lexer/scanner.cpp
    src/parser/parser.cpp
    src/parser/symbol_table.cpp
    src/preprocessor/preprocessor.cpp
    src/utils/file_utils.cpp
)
target_include_directories(compiler_core PUBLIC src)

add_executable(compiler src/main.cpp)
target_link_libraries(compiler PRIVATE compiler_core)

add_executable(lexer_test_runner tests/test_runner/main.cpp)
target_link_libraries(lexer_test_runner PRIVATE compiler_core)

# GCC < 9 requires explicit linking for std::filesystem
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(lexer_test_runner PRIVATE stdc++fs)
endif()

enable_testing()
add_test(
    NAME lexer_tests
    COMMAND lexer_test_runner
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexer/valid"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexer/invalid"
)

add_executable(parser_test_runner tests/test_runner/parser_test_runner.cpp)
target_link_libraries(parser_test_runner PRIVATE compiler_core)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(parser_test_runner PRIVATE stdc++fs)
endif()

add_test(
    NAME parser_tests
    COMMAND parser_test_runner
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parser/valid"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parser/invalid"
)

add_executable(integration_test_runner tests/test_runner/integration_test_runner.cpp)
target_link_libraries(integration_test_runner PRIVATE compiler_core)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(integration_test_runner PRIVATE stdc++fs)
endif()

add_test(
    NAME integration_examples
    COMMAND integration_test_runner
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/examples"
)
