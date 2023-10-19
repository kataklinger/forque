cmake_minimum_required(VERSION 3.26)

function(AddClangTidy target)
  if(CMAKE_CXX_CLANG_TIDY)
    set(CLANG_TIDY_PATH "${CMAKE_CXX_CLANG_TIDY}")
  else()
    find_program(CLANG_TIDY_PATH clang-tidy REQUIRED)
  endif()

  if(MSVC)
    list(APPEND CLANG_TIDY_PATH "--extra-arg=/EHsc")
  endif()

  set_target_properties(
    ${target} PROPERTIES
    CXX_CLANG_TIDY "${CLANG_TIDY_PATH}")
endfunction()
