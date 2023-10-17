cmake_minimum_required(VERSION 3.26)

project("forque" CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_subdirectory(src bin)
add_subdirectory(sample)

enable_testing() # has to be here so VS would detect test
add_subdirectory(test)

include(Install)

