cmake_minimum_required(VERSION 3.18)

add_library(forque STATIC mutex.cpp)

target_include_directories(forque PUBLIC inc)

target_compile_features(forque PUBLIC cxx_std_20)
set_target_properties(forque PROPERTIES
  CXX_EXTENSIONS NO
  CXX_STANDARD_REQUIRED YES)
