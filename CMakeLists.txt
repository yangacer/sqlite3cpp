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
cmake_minimum_required(VERSION 3.14)

project(sqlite3cpp VERSION 0.3.0 LANGUAGES CXX C)

include(FetchContent)
include(GenerateExportHeader)
include(FindThreads)

set(CMAKE_CXX_STANDARD 17)
set(SOURCE sqlite3cpp.cpp)
set(PUBHDR sqlite3cpp.h sqlite3cpp.ipp ${PROJECT_BINARY_DIR}/sqlite3cpp_export.h)

#
# Deps
FetchContent_Declare(sqlite3 URL "https://www.sqlite.org/2022/sqlite-amalgamation-3380500.zip")
if (NOT sqlite3_POPULATED)
  # notice that we do not call `add_subdirectory`, we simply fetch and then
  # use the subdirectory sources directly.
  FetchContent_Populate(sqlite3)
endif()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.11.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

configure_file(version.h.in ${PROJECT_SOURCE_DIR}/version.h)

#
# Lib sqlite3cpp
add_library(sqlite3cpp)
generate_export_header(sqlite3cpp)
target_include_directories(sqlite3cpp
  PUBLIC
    ${PROJECT_BINARY_DIR} # NOTE(acer) For sqlite3cpp_export.h
    $<BUILD_INTERFACE:${sqlite3_SOURCE_DIR}>
    )
target_sources(sqlite3cpp
  PRIVATE ${SOURCE} ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_link_libraries(sqlite3cpp ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(sqlite3cpp PROPERTIES PUBLIC_HEADER "${PUBHDR}")

# Lib RTTI configuration
if (MSVC)
  target_compile_options(sqlite3cpp PRIVATE /GR-)
else()
  target_compile_options(sqlite3cpp PRIVATE -fno-rtti)
endif()

add_executable(unittest gtest.cpp)
target_include_directories(unittest PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(unittest gtest_main sqlite3cpp)

install(TARGETS sqlite3cpp
  LIBRARY DESTINATION lib CONFIGURATIONS Release
  ARCHIVE DESTINATION lib CONFIGURATIONS Release
  RUNTIME DESTINATION lib CONFIGURATIONS Release
  PUBLIC_HEADER DESTINATION include CONFIGURATIONS Release)

add_executable(bench bench.cpp)
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
add_test(unittests ./unittest --gtest_shuffle --gtest_repeat=2)
