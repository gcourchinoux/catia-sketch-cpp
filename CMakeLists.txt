cmake_minimum_required(VERSION 3.20)
project(catia_sketch_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(catia_sketch
    src/catia_sketch_parser.cpp
)
target_include_directories(catia_sketch PUBLIC include)
target_compile_features(catia_sketch PUBLIC cxx_std_20)
target_compile_options(catia_sketch PRIVATE -Wall -Wextra -Wpedantic)

add_executable(catia-sketch-read examples/read_sketch.cpp)
target_link_libraries(catia-sketch-read PRIVATE catia_sketch)

include(CTest)
if(BUILD_TESTING)
    add_executable(catia-sketch-tests tests/parser_tests.cpp)
    target_link_libraries(catia-sketch-tests PRIVATE catia_sketch)
    add_test(NAME catia-sketch-tests COMMAND catia-sketch-tests)
endif()
