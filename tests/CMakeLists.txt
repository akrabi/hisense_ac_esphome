cmake_minimum_required(VERSION 3.20)
project(hisense_native_tests LANGUAGES CXX)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set(COMPONENT "${CMAKE_CURRENT_SOURCE_DIR}/../components/hisense_ac")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_native.py"
          "${CMAKE_CURRENT_BINARY_DIR}"
  COMMAND_ERROR_IS_FATAL ANY)
add_executable(command_tests "${CMAKE_CURRENT_BINARY_DIR}/command_tests.cpp"
               "${COMPONENT}/commands.cpp")
target_include_directories(command_tests PRIVATE "${COMPONENT}")
add_test(NAME command_golden COMMAND command_tests)
add_executable(legacy_parser_tests "${CMAKE_CURRENT_BINARY_DIR}/legacy_parser_tests.cpp")
add_test(NAME legacy_parser_characterization COMMAND legacy_parser_tests)
