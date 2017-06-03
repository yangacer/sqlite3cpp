cmake_minimum_required(VERSION 3.0)
include("cmake/HunterGate.cmake")

HunterGate(
  URL "https://github.com/ruslo/hunter/archive/v0.19.0.tar.gz"
  SHA1 "e9d11aee66607cfcfbec5e3811d4804135b5bfac"
  )

project(sqlite3cpp VERSION 0.1.1)

hunter_add_package(GTest)

find_package(GTest CONFIG REQUIRED)

configure_file(version.h.in ${PROJECT_SOURCE_DIR}/version.h)

set(SOURCE sqlite3cpp.cpp stringpiece.cc)
add_library(sqlite3cpp SHARED ${SOURCE})
target_link_libraries(sqlite3cpp PUBLIC sqlite3)
set_target_properties(sqlite3cpp PROPERTIES
  CXX_STANDARD 14
  PUBLIC_HEADER "sqlite3cpp.h;sqlite3cpp.ipp;stringpiece.h")

if(MSVC)
  add_custom_target(sqlite3_genlib ALL lib /DEF:win\\sqlite3.def /OUT:${PROJECT_BINARY_DIR}\\sqlite3.lib
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  include(GenerateExportHeader)
  generate_export_header(sqlite3cpp)
  # NOTE(acer) Include P_B_DIR for sqlite3cpp_export.h
  include_directories(win ${PROJECT_BINARY_DIR})
  link_directories(${PROJECT_BINARY_DIR})
endif()


add_executable(gtest gtest.cpp)
set_target_properties(gtest
  PROPERTIES CXX_STANDARD 14)
target_link_libraries(gtest PUBLIC GTest::main sqlite3cpp )

install(TARGETS sqlite3cpp
  LIBRARY DESTINATION lib CONFIGURATIONS Release
  RUNTIME DESTINATION lib CONFIGURATIONS Release
  PUBLIC_HEADER DESTINATION include CONFIGURATIONS Release)

if(NOT MSVC)
  add_executable(bench bench.cpp)
  set_target_properties(bench PROPERTIES CXX_STANDARD 14)
  target_link_libraries(bench sqlite3cpp)
endif()
#
# Coversall configuration

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/coveralls/cmake)

option(COVERALLS "Generate coveralls data" OFF)

if (COVERALLS)
  include(Coveralls)
  coveralls_turn_on_coverage()
  set(COVERAGE_SRCS sqlite3cpp.cpp sqlite3cpp.ipp)
  coveralls_setup(
    "${COVERAGE_SRCS}" # The source files.
    ON)                 # If we should upload.
endif()

enable_testing()
add_test(unittest ./gtest --gtest_shuffle --gtest_repeat=2)
