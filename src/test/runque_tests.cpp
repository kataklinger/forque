
#include "../inc/runque.hpp"
#include "../inc/sync_wait.hpp"

#include "gtest/gtest.h"

namespace {
class test_item {
public:
  constexpr test_item() = default;

  constexpr inline test_item(int v1, int v2) noexcept
      : v1_{v1}
      , v2_{v2} {
  }

  constexpr test_item(test_item&& item) = default;
  constexpr test_item& operator=(test_item&&) = default;

  test_item(test_item const&) = delete;
  test_item& operator=(test_item const&) = delete;

  constexpr auto operator<=>(const test_item&) const = default;

private:
  int v1_{};
  int v2_{};
};
} // namespace

// priority queue tests

class runque_queue_priority_empty_test : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::priority_runque_queue<test_item> queue_{};
};

TEST_F(runque_queue_priority_empty_test, is_empty_when_empty) {
  EXPECT_TRUE(queue_.empty());
}

TEST_F(runque_queue_priority_empty_test, move_push_to_empty) {
  queue_.push({});
  EXPECT_FALSE(queue_.empty());
}

TEST_F(runque_queue_priority_empty_test, emplace_to_empty) {
  queue_.push(1, 1);
  EXPECT_FALSE(queue_.empty());
}

class runque_queue_priority_nonempty_test : public testing::Test {
protected:
  void SetUp() override {
    queue_.push(5, 5);
  }

  frq::priority_runque_queue<test_item> queue_{};
};

TEST_F(runque_queue_priority_nonempty_test, is_empty_when_nonempty) {
  EXPECT_FALSE(queue_.empty());
}

TEST_F(runque_queue_priority_nonempty_test, is_empty_after_pop_last) {
  queue_.pop();

  EXPECT_TRUE(queue_.empty());
}

TEST_F(runque_queue_priority_nonempty_test, pop_from_nonempty) {
  constexpr test_item expected{5, 5};
  EXPECT_EQ(expected, queue_.pop());
}

TEST_F(runque_queue_priority_nonempty_test, push_after_top) {
  constexpr test_item expected{5, 5};

  queue_.push(4, 4);

  EXPECT_EQ(expected, queue_.pop());
}

TEST_F(runque_queue_priority_nonempty_test, push_before_top) {
  constexpr test_item expected{6, 6};

  queue_.push(6, 6);

  EXPECT_EQ(expected, queue_.pop());
}

// fifo queue tests

class runque_queue_fifo_empty_test : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::fifo_runque_queue<test_item> queue_{};
};

TEST_F(runque_queue_fifo_empty_test, is_empty_when_empty) {
  EXPECT_TRUE(queue_.empty());
}

TEST_F(runque_queue_fifo_empty_test, move_push_to_empty) {
  queue_.push({});
  EXPECT_FALSE(queue_.empty());
}

TEST_F(runque_queue_fifo_empty_test, emplace_to_empty) {
  queue_.push(1, 1);
  EXPECT_FALSE(queue_.empty());
}

class runque_queue_fifo_nonempty_test : public testing::Test {
protected:
  void SetUp() override {
    queue_.push(5, 5);
  }

  frq::fifo_runque_queue<test_item> queue_{};
};

TEST_F(runque_queue_fifo_nonempty_test, is_empty_when_nonempty) {
  EXPECT_FALSE(queue_.empty());
}

TEST_F(runque_queue_fifo_nonempty_test, is_empty_after_pop_last) {
  queue_.pop();

  EXPECT_TRUE(queue_.empty());
}

TEST_F(runque_queue_fifo_nonempty_test, pop_from_nonempty) {
  constexpr test_item expected{5, 5};
  EXPECT_EQ(expected, queue_.pop());
}

TEST_F(runque_queue_fifo_nonempty_test, push_to_nonempty) {
  constexpr test_item expected{5, 5};

  queue_.push(6, 6);

  EXPECT_EQ(expected, queue_.pop());
}

// lifo queue tests

class runque_queue_lifo_empty_test : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::lifo_runque_queue<test_item> queue_{};
};

TEST_F(runque_queue_lifo_empty_test, is_empty_when_empty) {
  EXPECT_TRUE(queue_.empty());
}

TEST_F(runque_queue_lifo_empty_test, move_push_to_empty) {
  queue_.push({});
  EXPECT_FALSE(queue_.empty());
}

TEST_F(runque_queue_lifo_empty_test, emplace_to_empty) {
  queue_.push(1, 1);
  EXPECT_FALSE(queue_.empty());
}

class runque_queue_lifo_nonempty_test : public testing::Test {
protected:
  void SetUp() override {
    queue_.push(5, 5);
  }

  frq::lifo_runque_queue<test_item> queue_{};
};

TEST_F(runque_queue_lifo_nonempty_test, is_empty_when_nonempty) {
  EXPECT_FALSE(queue_.empty());
}

TEST_F(runque_queue_lifo_nonempty_test, is_empty_after_pop_last) {
  queue_.pop();

  EXPECT_TRUE(queue_.empty());
}

TEST_F(runque_queue_lifo_nonempty_test, pop_from_nonempty) {
  constexpr test_item expected{5, 5};
  EXPECT_EQ(expected, queue_.pop());
}

TEST_F(runque_queue_lifo_nonempty_test, push_to_nonempty) {
  constexpr test_item expected{6, 6};

  queue_.push(6, 6);

  EXPECT_EQ(expected, queue_.pop());
}

// runque single threaded

class runque_single_threaded_empty_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::runque<frq::fifo_runque_queue<test_item>, frq::single_thread_model>
      runque_{};
};

TEST_F(runque_single_threaded_empty_tests, get_from_empty) {
  auto result = runque_.get();

  EXPECT_FALSE(result.has_value());
}

class runque_single_threaded_interrupted
    : public runque_single_threaded_empty_tests {
protected:
  void SetUp() override {
    runque_.interrupt();
  }
};

TEST_F(runque_single_threaded_interrupted, get) {
  EXPECT_THROW(runque_.get(), frq::interrupted);
}

TEST_F(runque_single_threaded_interrupted, put) {
  EXPECT_THROW(runque_.put(1, 1), frq::interrupted);
}

class runque_single_threaded_nonempty_tests : public testing::Test {
protected:
  void SetUp() override {
    runque_.put(1, 1);
  }

  frq::runque<frq::fifo_runque_queue<test_item>, frq::single_thread_model>
      runque_{};
};

TEST_F(runque_single_threaded_nonempty_tests, get_from_nonempty) {
  constexpr test_item expected{1, 1};

  auto result = runque_.get();

  EXPECT_EQ(expected, *result);
}

// runque coro

class runque_coro_empty_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::runque<frq::fifo_runque_queue<test_item>, frq::coro_thread_model>
      runque_{};
};

TEST_F(runque_coro_empty_tests, get_before_put) {
  constexpr test_item expected{1, 1};

  test_item result;

  auto getter = [this, &result]() -> frq::task<> {
    result = std::move(co_await runque_.get());
  }();

  std::thread{[&getter]() { getter.start(); }}.join();

  frq::sync_wait(runque_.put({1, 1}));

  EXPECT_EQ(expected, result);
}

TEST_F(runque_coro_empty_tests, put_before_get) {
  constexpr test_item expected{1, 1};

  frq::sync_wait(runque_.put({1, 1}));
  test_item result{frq::sync_wait(runque_.get())};

  EXPECT_EQ(expected, result);
}

TEST_F(runque_coro_empty_tests, get_before_emplace) {
  constexpr test_item expected{1, 1};

  test_item result;

  auto getter = [this, &result]() -> frq::task<> {
    result = std::move(co_await runque_.get());
  }();

  std::thread{[&getter]() { getter.start(); }}.join();

  frq::sync_wait(runque_.put(1, 1));

  EXPECT_EQ(expected, result);
}

TEST_F(runque_coro_empty_tests, empalce_before_get) {
  constexpr test_item expected{1, 1};

  frq::sync_wait(runque_.put(1, 1));
  test_item result{frq::sync_wait(runque_.get())};

  EXPECT_EQ(expected, result);
}

TEST_F(runque_coro_empty_tests, get_before_interrupt) {
  auto getter = [this]() -> frq::task<> {
    EXPECT_THROW(co_await runque_.get(), frq::interrupted);
  }();

  std::thread{[&getter]() { getter.start(); }}.join();

  frq::sync_wait(runque_.interrupt());
}

TEST_F(runque_coro_empty_tests, interrupt_before_get) {
  frq::sync_wait(runque_.interrupt());
  EXPECT_THROW(frq::sync_wait(runque_.get()), frq::interrupted);
}

TEST_F(runque_coro_empty_tests, interrupt_before_put) {
  frq::sync_wait(runque_.interrupt());
  EXPECT_THROW(frq::sync_wait(runque_.put({1, 1})), frq::interrupted);
}

TEST_F(runque_coro_empty_tests, interrupt_before_emplace) {
  frq::sync_wait(runque_.interrupt());
  EXPECT_THROW(frq::sync_wait(runque_.put(1, 1)), frq::interrupted);
}
