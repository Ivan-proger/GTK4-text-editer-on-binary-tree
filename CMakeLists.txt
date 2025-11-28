# CMakeLists.txt (root)
cmake_minimum_required(VERSION 3.16)
project(BinaryTreeEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Опция: собирать тесты
option(BUILD_TESTS "Build tests" ON)

add_subdirectory(src)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
