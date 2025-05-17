cmake_minimum_required(VERSION 4.3)
project(OrdinaryJson)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(OrdinaryJson
        src/example.hpp
        src/main.cpp
        src/ordinary_json.cpp
        src/ordinary_json.hpp)
