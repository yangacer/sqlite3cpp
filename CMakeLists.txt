cmake_minimum_required(VERSION 3.0)
include("cmake/HunterGate.cmake")

HunterGate(
  URL "https://github.com/ruslo/hunter/archive/v0.12.47.tar.gz"
  SHA1 "3f7dd82d1eb47633ea01a9f7bbcc51b0e8ada1d9"
  )

project(sqlite3cpp VERSION 0.1.1)

hunter_add_package(GTest)

find_package(GTest CONFIG REQUIRED)

configure_file(version.h.in ${PROJECT_SOURCE_DIR}/version.h)

include(GenerateExportHeader)
set(SOURCE sqlite3cpp.cpp stringpiece.cc)
set(PUBHDR sqlite3cpp.h sqlite3cpp.ipp stringpiece.h ${PROJECT_BINARY_DIR}/sqlite3cpp_export.h)

add_library(sqlite3cpp SHARED ${SOURCE})
generate_export_header(sqlite3cpp)
# NOTE(acer) For sqlite3cpp_export.h
include_directories(${PROJECT_BINARY_DIR})
target_link_libraries(sqlite3cpp PUBLIC sqlite3)
set_target_properties(sqlite3cpp PROPERTIES
  CXX_STANDARD 14
  PUBLIC_HEADER "${PUBHDR}")

if(MSVC)
  # TODO(acer): Download specific versoin of sqlite3 via cmake external project
  link_directories(${PROJECT_BINARY_DIR})
  include_directories(win)
  # TODO: ARM target
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    # 64bit
    configure_file(win/x64/sqlite3.dll ${PROJECT_BINARY_DIR}/sqlite3.dll COPYONLY)
    # Generate sqlite3.lib from sqlite3.def
    add_custom_target(sqlite3_genlib ALL lib /DEF:win\\sqlite3.def /MACHINE:x64 /OUT:${PROJECT_BINARY_DIR}\\sqlite3.lib
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  else()
    # 32bit
    configure_file(win/x86/sqlite3.dll ${PROJECT_BINARY_DIR}/sqlite3.dll COPYONLY)
    # Generate sqlite3.lib from sqlite3.def
    add_custom_target(sqlite3_genlib ALL lib /DEF:win\\sqlite3.def /MACHINE:X86 /OUT:${PROJECT_BINARY_DIR}\\sqlite3.lib
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  endif()
endif()


add_executable(gtest gtest.cpp)
set_target_properties(gtest
  PROPERTIES CXX_STANDARD 14)
target_link_libraries(gtest PUBLIC GTest::main sqlite3cpp )

install(TARGETS sqlite3cpp
  LIBRARY DESTINATION lib CONFIGURATIONS Release
  RUNTIME DESTINATION lib CONFIGURATIONS Release
  PUBLIC_HEADER DESTINATION include CONFIGURATIONS Release)

add_executable(bench bench.cpp)
set_target_properties(bench PROPERTIES CXX_STANDARD 14)
target_link_libraries(bench sqlite3cpp)
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
