cmake_minimum_required(VERSION 3.18)

find_package(Threads)

add_executable(app app.cpp pool.cpp)

target_link_libraries(app PRIVATE forque ${CMAKE_THREAD_LIBS_INIT})

target_compile_features(app PRIVATE cxx_std_20)
set_target_properties(app PROPERTIES
  CXX_EXTENSIONS NO
  CXX_STANDARD_REQUIRED YES)
