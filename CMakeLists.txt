cmake_minimum_required(VERSION 4.3)
project(OrdinaryJson)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

add_executable(OrdinaryJson
        src/main.cpp
        src/ordinary_json.cpp
        src/ordinary_json.hpp)

enable_testing()

add_executable(ordinaryjson_tests
        tests/test_ordinary_json.cpp
        src/ordinary_json.cpp
        src/ordinary_json.hpp)

add_test(NAME ordinaryjson_tests COMMAND ordinaryjson_tests)
