# CMake generated Testfile for 
# Source directory: C:/school/priklad/6sem/compiler-project
# Build directory: C:/school/priklad/6sem/compiler-project/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(lexer_tests "C:/school/priklad/6sem/compiler-project/build/Debug/lexer_test_runner.exe" "C:/school/priklad/6sem/compiler-project/tests/lexer/valid" "C:/school/priklad/6sem/compiler-project/tests/lexer/invalid")
  set_tests_properties(lexer_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/school/priklad/6sem/compiler-project/CMakeLists.txt;22;add_test;C:/school/priklad/6sem/compiler-project/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(lexer_tests "C:/school/priklad/6sem/compiler-project/build/Release/lexer_test_runner.exe" "C:/school/priklad/6sem/compiler-project/tests/lexer/valid" "C:/school/priklad/6sem/compiler-project/tests/lexer/invalid")
  set_tests_properties(lexer_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/school/priklad/6sem/compiler-project/CMakeLists.txt;22;add_test;C:/school/priklad/6sem/compiler-project/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(lexer_tests "C:/school/priklad/6sem/compiler-project/build/MinSizeRel/lexer_test_runner.exe" "C:/school/priklad/6sem/compiler-project/tests/lexer/valid" "C:/school/priklad/6sem/compiler-project/tests/lexer/invalid")
  set_tests_properties(lexer_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/school/priklad/6sem/compiler-project/CMakeLists.txt;22;add_test;C:/school/priklad/6sem/compiler-project/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(lexer_tests "C:/school/priklad/6sem/compiler-project/build/RelWithDebInfo/lexer_test_runner.exe" "C:/school/priklad/6sem/compiler-project/tests/lexer/valid" "C:/school/priklad/6sem/compiler-project/tests/lexer/invalid")
  set_tests_properties(lexer_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/school/priklad/6sem/compiler-project/CMakeLists.txt;22;add_test;C:/school/priklad/6sem/compiler-project/CMakeLists.txt;0;")
else()
  add_test(lexer_tests NOT_AVAILABLE)
endif()
