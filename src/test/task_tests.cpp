
#include "..\inc\sync_wait.hpp"
#include "..\inc\task.hpp"

#include "counted.hpp"

#include "gtest/gtest.h"

TEST(task_tests, lazy_start) {
  auto executed{false};

  auto task = [&executed]() -> frq::task<> {
    executed = true;
    co_return;
  }();

  EXPECT_FALSE(executed);

  frq::sync_wait(task);

  EXPECT_TRUE(executed);
}

TEST(task_tests, args_cleanup) {
  counted_guard guard{};

  {
    auto task = [](counted c) -> frq::task<> {
      EXPECT_EQ(1, counted::get_instances());
      co_return;
    }(guard.instance());

    EXPECT_EQ(1, counted::get_instances());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(task_tests, result_cleanup) {
  counted_guard guard{};

  {
    auto result = frq::sync_wait(
        [&guard]() -> frq::task<counted> { co_return guard.instance(); }());

    EXPECT_EQ(1, counted::get_instances());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(task_tests, result_no_copies) {
  counted_guard guard{};

  {
    auto result = frq::sync_wait(
        [&guard]() -> frq::task<counted> { co_return guard.instance(); }());
  }

  EXPECT_EQ(0, counted::get_copies());
}

TEST(task_tests, async_start) {

  class {
  public:
    inline bool await_ready() const noexcept {
      return false;
    }

    inline void await_suspend(std::coroutine_handle<> continuation) noexcept {
      continuation_ = continuation;
    }

    inline void await_resume() const noexcept {
    }

    inline void resume() const noexcept {
      continuation_.resume();
    }

  private:
    std::coroutine_handle<> continuation_;
  } awaitable;

  auto before{false}, after{false};

  auto late = [&]() -> frq::task<> { co_await awaitable; };

  auto early = [&]() -> frq::task<> {
    before = true;
    co_await late();
    after = true;
  }();

  std::thread{[&]() {
    early.start();

    EXPECT_TRUE(before);
    EXPECT_FALSE(after);
  }}.join();

  awaitable.resume();

  EXPECT_TRUE(before);
  EXPECT_TRUE(after);
}
