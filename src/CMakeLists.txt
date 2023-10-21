cmake_minimum_required(VERSION 3.26)

add_library(warnings INTERFACE)

if(MSVC)
  target_compile_options(warnings INTERFACE /W4 /WX)
else()
  target_compile_options(warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(forque STATIC mutex.cpp)

target_link_libraries(forque PRIVATE warnings)

target_include_directories(
  forque PUBLIC
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>)

list(APPEND HEADER_LIST
    forque.hpp
    mutex.hpp
    runque.hpp
    sync_wait.hpp
    tag.hpp
    tag_stream.hpp
    task.hpp
    utility.hpp)

list(TRANSFORM HEADER_LIST PREPEND $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
list(TRANSFORM HEADER_LIST PREPEND $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/>)

target_sources(
  forque PUBLIC
  FILE_SET forque_headers
  TYPE HEADERS
  FILES ${HEADER_LIST}
  BASE_DIRS
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>)

target_compile_features(forque PUBLIC cxx_std_20)
set_target_properties(
  forque PROPERTIES
  CXX_EXTENSIONS NO
  CXX_STANDARD_REQUIRED YES)

include(ClangTidy)
AddClangTidy(forque)

include(CppCheck)
AddCppCheck(forque)
