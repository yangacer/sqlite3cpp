cmake_minimum_required(VERSION 3.0)
include("cmake/HunterGate.cmake")

HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.12.47.tar.gz"
    SHA1 "3f7dd82d1eb47633ea01a9f7bbcc51b0e8ada1d9"
    )

project(sqlite3cpp VERSION 0.1.0)

add_library(sqlite3cpp SHARED sqlite3cpp.cpp)
target_link_libraries(sqlite3cpp PUBLIC sqlite3)
set_property(TARGET sqlite3cpp PROPERTY CXX_STANDARD 14)

hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)
add_executable(gtest gtest.cpp)
set_property(TARGET gtest PROPERTY CXX_STANDARD 14)
target_link_libraries(gtest PUBLIC GTest::main sqlite3cpp)

