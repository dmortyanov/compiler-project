cmake_minimum_required(VERSION 3.10)
project(MiniCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(compiler_core
    src/lexer/token.cpp
    src/lexer/scanner.cpp
    src/preprocessor/preprocessor.cpp
    src/utils/file_utils.cpp
)
target_include_directories(compiler_core PUBLIC src)

add_executable(compiler src/main.cpp)
target_link_libraries(compiler PRIVATE compiler_core)

add_executable(lexer_test_runner tests/test_runner/main.cpp)
target_link_libraries(lexer_test_runner PRIVATE compiler_core)

enable_testing()
add_test(
    NAME lexer_tests
    COMMAND lexer_test_runner
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexer/valid"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexer/invalid"
)
