################################################################################
#  Copyright (c) 2019, Acer Yun-Tse Yang All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#  this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice,
#  this list of conditions and the following disclaimer in the documentation
#  and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#  contributors may be used to endorse or promote products derived from this
#  software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
################################################################################
cmake_minimum_required(VERSION 3.0)
include("cmake/HunterGate.cmake")

HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.22.16.tar.gz"
    SHA1 "84153076a3cebf4869c904fa5c93ea309386b583"
)

project(sqlite3cpp VERSION 0.3.0)

hunter_add_package(GTest)
hunter_add_package(sqlite3)

find_package(GTest CONFIG REQUIRED)
find_package(sqlite3 3.21 CONFIG REQUIRED)

configure_file(version.h.in ${PROJECT_SOURCE_DIR}/version.h)

include(GenerateExportHeader)
set(SOURCE sqlite3cpp.cpp)
set(PUBHDR sqlite3cpp.h sqlite3cpp.ipp ${PROJECT_BINARY_DIR}/sqlite3cpp_export.h)

#
# Lib sqlite3cpp
add_library(sqlite3cpp STATIC ${SOURCE})
generate_export_header(sqlite3cpp)
include_directories(${PROJECT_BINARY_DIR}) # NOTE(acer) For sqlite3cpp_export.h
target_link_libraries(sqlite3cpp PUBLIC sqlite3::sqlite3)
set_target_properties(sqlite3cpp PROPERTIES
  CXX_STANDARD 17
  PUBLIC_HEADER "${PUBHDR}")

# Lib RTTI configuration
if (MSVC)
  target_compile_options(sqlite3cpp PRIVATE /GR-)
else()
  target_compile_options(sqlite3cpp PRIVATE -fno-rtti)
endif()

add_executable(gtest gtest.cpp)
set_target_properties(gtest
  PROPERTIES CXX_STANDARD 17)
target_link_libraries(gtest GTest::main sqlite3cpp)

install(TARGETS sqlite3cpp
  LIBRARY DESTINATION lib CONFIGURATIONS Release
  ARCHIVE DESTINATION lib CONFIGURATIONS Release
  RUNTIME DESTINATION lib CONFIGURATIONS Release
  PUBLIC_HEADER DESTINATION include CONFIGURATIONS Release)

add_executable(bench bench.cpp)
set_target_properties(bench PROPERTIES CXX_STANDARD 17)
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
