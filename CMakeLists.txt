cmake_minimum_required(VERSION 3.18)

project("forque" CXX)

# Forque Library

add_library(forque STATIC
  ./src/lib/mutex.cpp
  ./src/inc/forque.hpp
  ./src/inc/runque.hpp
  ./src/inc/tag_stream.hpp
  ./src/inc/tag.hpp
  ./src/inc/mutex.hpp
  ./src/inc/sync_wait.hpp
  ./src/inc/task.hpp
  ./src/inc/utility.hpp
  ./src/inc/sdefs.hpp)

target_include_directories(forque PUBLIC ./src/inc)

target_compile_features(forque PUBLIC cxx_std_20)
set_target_properties(forque PROPERTIES
  CXX_EXTENSIONS NO
  CXX_STANDARD_REQUIRED YES)

target_compile_options(forque PRIVATE ${FRQ_LIB_COMPILER_FLAGS})

# Test App

find_package(Threads)

add_executable(app
  ./src/app/app.cpp
  ./src/app/pool.cpp
  ./src/app/pool.hpp)

target_link_libraries(app PRIVATE forque ${CMAKE_THREAD_LIBS_INIT})

target_compile_features(app PUBLIC cxx_std_20)
set_target_properties(app PROPERTIES
  CXX_EXTENSIONS NO
  CXX_STANDARD_REQUIRED YES)

target_compile_options(app PRIVATE ${FRQ_LIB_COMPILER_FLAGS})

# GTest

enable_testing()
find_package(GTest CONFIG REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(tests
  ./src/test/forque_tests.cpp
  ./src/test/mutex_tests.cpp
  ./src/test/runque_tests.cpp
  ./src/test/sync_wait_tests.cpp
  ./src/test/tag_tests.cpp
  ./src/test/task_tests.cpp
  ./src/test/counted.hpp)

target_link_libraries(tests PRIVATE GTest::gtest GTest::gtest_main)
target_link_libraries(tests PRIVATE forque)

target_compile_features(tests PUBLIC cxx_std_20)
set_target_properties(tests PROPERTIES
  CXX_EXTENSIONS NO
  CXX_STANDARD_REQUIRED YES)

target_compile_options(tests PRIVATE ${FRQ_TEST_COMPILER_FLAGS})

include(GoogleTest)
gtest_discover_tests(tests)