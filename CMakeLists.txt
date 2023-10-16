cmake_minimum_required(VERSION 3.18)

project("forque" CXX)

add_subdirectory(src bin)
add_subdirectory(sample)

enable_testing() # has to be here so VS would detect test
add_subdirectory(test)
