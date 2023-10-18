cmake_minimum_required(VERSION 3.26)

function(AddClangTidy target)
  find_program(CLANG_TIDY_PATH clang-tidy REQUIRED)
  set_target_properties(
    ${target} PROPERTIES
    CXX_CLANG_TIDY "${CLANG_TIDY_PATH}")
endfunction()
