
#include "..\inc\sync_wait.hpp"
#include "..\inc\task.hpp"

#include "gtest/gtest.h"

TEST(sync_wait_tests, wait_void) {
  int count{0};

  frq::sync_wait([&count]() -> frq::task<> {
    ++count;
    co_return;
  }());

  EXPECT_EQ(1, count);
}

TEST(sync_wait_tests, wait_arithmetic_lvalue) {
  auto task = []() -> frq::task<int> { co_return 7; }();
  decltype(auto) result = frq::sync_wait(task);

  static_assert(std::is_same_v<int&, decltype(result)>);
  EXPECT_EQ(7, result);
}

TEST(sync_wait_tests, wait_arithmetic_rvalue) {
  auto result = frq::sync_wait([]() -> frq::task<int> { co_return 7; }());

  static_assert(std::is_same_v<int, decltype(result)>);
  EXPECT_EQ(7, result);
}

TEST(sync_wait_tests, wait_lvalue_ref) {
  int target{0};

  decltype(auto) result =
      frq::sync_wait([&target]() -> frq::task<int&> { co_return target; }());

  static_assert(std::is_lvalue_reference_v<decltype(result)>);
  EXPECT_EQ(&target, &result);
}

TEST(sync_wait_tests, wait_lvalue_ref_const) {
  int target{0};

  decltype(auto) result = frq::sync_wait(
      [&target]() -> frq::task<const int&> { co_return target; }());

  static_assert(std::is_lvalue_reference_v<decltype(result)> &&
                std::is_const_v<std::remove_reference_t<decltype(result)>>);
  EXPECT_EQ(&target, &result);
}

TEST(sync_wait_tests, wait_rvalue_ref) {
  int target{0};

  decltype(auto) result = frq::sync_wait(
      [&target]() -> frq::task<int&&> { co_return std::move(target); }());

  static_assert(std::is_rvalue_reference_v<decltype(result)>);
  EXPECT_EQ(&target, &result);
}

TEST(sync_wait_tests, wait_exception) {
   EXPECT_ANY_THROW(frq::sync_wait([]() -> frq::task<> {
    throw std::exception();
    co_return;
  }()));
}

TEST(sync_wait_tests, wait_completed) {
  int count{0};
  auto task = [&count]() -> frq::task<> {
    ++count;
    co_return;
  }();

  frq::sync_wait(task);
  frq::sync_wait(task);

  EXPECT_EQ(1, count);
}
