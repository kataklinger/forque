cmake_minimum_required(VERSION 3.26)

include(GNUInstallDirs)

install(
  TARGETS forque
  EXPORT forque_export
  FILE_SET forque_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/forque)

install(
  EXPORT forque_export
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/forque/cmake
  NAMESPACE frq::)

install(
  FILES ./cmake/forqueConfig.cmake
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/forque/cmake)
