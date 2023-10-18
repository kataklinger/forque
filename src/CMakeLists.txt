cmake_minimum_required(VERSION 3.26)

add_library(warnings INTERFACE)

if(MSVC)
  target_compile_options(warnings INTERFACE /W4 /WX)
else()
  target_compile_options(warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(forque STATIC mutex.cpp)

target_link_libraries(forque PRIVATE warnings)

target_include_directories(forque PUBLIC
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>)

target_sources(
  forque PUBLIC
  FILE_SET forque_headers
  TYPE HEADERS
  FILES
    $<INSTALL_INTERFACE:forque.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/forque.hpp>
    $<INSTALL_INTERFACE:mutex.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/mutex.hpp>
    $<INSTALL_INTERFACE:runque.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/runque.hpp>
    $<INSTALL_INTERFACE:sync_wait.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/sync_wait.hpp>
    $<INSTALL_INTERFACE:tag.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/tag.hpp>
    $<INSTALL_INTERFACE:tag_stream.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/tag_stream.hpp>
    $<INSTALL_INTERFACE:task.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/task.hpp>
    $<INSTALL_INTERFACE:utility.hpp> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/utility.hpp>
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/inc)

target_compile_features(forque PUBLIC cxx_std_20)
set_target_properties(
  forque PROPERTIES
  CXX_EXTENSIONS NO
  CXX_STANDARD_REQUIRED YES)

if(NOT MSVC)
  include(ClangTidy)
  AddClangTidy(forque)
endif()

include(CppCheck)
AddCppCheck(forque)
