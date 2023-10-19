
#include "../inc/mutex.hpp"
#include "../inc/sync_wait.hpp"
#include "../inc/task.hpp"

#include "gtest/gtest.h"

#include <thread>

class mutex_unlocked_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::mutex mutex_;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-capturing-lambda-coroutines,cppcoreguidelines-avoid-reference-coroutine-parameters)

TEST_F(mutex_unlocked_tests, try_lock_unlocked_mutex) {
  EXPECT_TRUE(mutex_.try_lock());
}

TEST_F(mutex_unlocked_tests, unlock_unlocked_mutex) {
  EXPECT_DEATH(mutex_.unlock(), "is_free");
}

class mutex_locked_tests : public mutex_unlocked_tests {
protected:
  void SetUp() override {
    assert(mutex_.try_lock());
  }
};

TEST_F(mutex_locked_tests, try_lock_unlocked_mutex) {
  EXPECT_FALSE(mutex_.try_lock());
}

TEST_F(mutex_locked_tests, unlock_mutex) {
  mutex_.unlock();

  EXPECT_TRUE(mutex_.try_lock());
}

TEST_F(mutex_locked_tests, guarded_unlock_mutex) {
  { frq::mutex_guard const guard(mutex_, std::adopt_lock); }

  EXPECT_TRUE(mutex_.try_lock());
}

class mutex_multithreaded_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  template<typename Inner>
  void wrapper(std::size_t concurrency, std::size_t count, Inner inner) {
    auto body = [count, inner] {
      frq::sync_wait([count, inner]() -> frq::task<> {
        for (std::size_t i{0}; i < count; ++i) {
          co_await inner();
        }
      }());
    };

    std::vector<std::thread> threads{};
    for (std::size_t i = 0; i < concurrency; ++i) {
      threads.push_back(std::thread{body});
    }

    for (auto& thread : threads) {
      thread.join();
    }
  }

  frq::mutex mutex_;
  std::atomic<int> counter_;
};

TEST_F(mutex_multithreaded_tests, mutex_raw) {
  wrapper(8, 1000, [this]() -> frq::task<> {
    co_await mutex_.lock();

    EXPECT_EQ(1, ++counter_);
    EXPECT_EQ(0, --counter_);

    mutex_.unlock();
  });
}

TEST_F(mutex_multithreaded_tests, mutex_guard) {
  wrapper(8, 1000, [this]() -> frq::task<> {
    co_await mutex_.lock();
    frq::mutex_guard const guard{mutex_, std::adopt_lock};

    EXPECT_EQ(1, ++counter_);
    EXPECT_EQ(0, --counter_);
  });
}

// NOLINTEND(cppcoreguidelines-avoid-capturing-lambda-coroutines,cppcoreguidelines-avoid-reference-coroutine-parameters)
