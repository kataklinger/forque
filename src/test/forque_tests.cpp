
#include "../inc/forque.hpp"
#include "../inc/sync_wait.hpp"

#include "gtest/gtest.h"

#include <string>

using item_type = float;
using runque_type =
    frq::make_runque_t<frq::fifo_order,
                       frq::coro_thread_model,
                       frq::retainment<item_type>,
                       std::allocator<frq::retainment<item_type>>>;

using static_tag = frq::stag_t<int, float>;
using static_queue = frq::forque<item_type, runque_type, static_tag>;

using dynamic_tag = frq::dtag<>;
using dynamic_queue = frq::forque<item_type, runque_type, dynamic_tag>;

using reservation_type = frq::reservation<item_type>;
using retainment_type = frq::retainment<item_type>;

template<typename Ty>
struct is_task : std::false_type {};

template<typename Ty>
struct is_task<frq::task<Ty>> : std::true_type {};

template<typename Ty>
inline constexpr bool is_task_v = is_task<Ty>::value;

template<typename Ty>
concept wrap_callable = requires(Ty t) {
  {std::forward<Ty>(t)()};
};

class static_queue_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  template<typename Fn, typename... Args>
  frq::task<> push(Fn&& wrapped,
                   item_type const& value,
                   Args&&... args) requires(wrap_callable<Fn>) {
    using tag_type = frq::sub_tag_t<static_tag, sizeof...(Args)>;

    auto reservation = co_await queue_.reserve(
        tag_type{frq::construct_tag_default, std::forward<Args>(args)...});

    if constexpr (is_task_v<std::decay_t<std::invoke_result_t<Fn>>>) {
      co_await std::forward<Fn>(wrapped)();
    }
    else {
      std::forward<Fn>(wrapped)();
    }

    co_await reservation.release(value);
  }

  template<typename... Args>
  frq::task<> push(item_type const& value, Args&&... args) {
    return push([] {}, value, std::forward<Args>(args)...);
  }

  template<typename Fn, typename... Args>
  void push_sync(Fn&& wrapped,
                 item_type value,
                 Args&&... args) requires(wrap_callable<Fn>) {
    frq::sync_wait(
        push(std::forward<Fn>(wrapped), value, std::forward<Args>(args)...));
  }

  template<typename... Args>
  void push_sync(item_type value, Args&&... args) {
    frq::sync_wait(push(value, std::forward<Args>(args)...));
  }

  template<typename Fn>
  frq::task<item_type> pop(Fn&& wrapped) requires(wrap_callable<Fn>) {
    auto retainment = co_await queue_.get();
    auto result = retainment.value();

    if constexpr (is_task_v<std::decay_t<std::invoke_result_t<Fn>>>) {
      co_await std::forward<Fn>(wrapped)();
    }
    else {
      std::forward<Fn>(wrapped)();
    }

    co_await retainment.finalize();
    co_return result;
  }

  frq::task<item_type> pop() {
    return pop([] {});
  }

  template<typename Fn>
  item_type pop_sync(Fn&& wrapped) requires(wrap_callable<Fn>) {
    return frq::sync_wait(pop(std::forward<Fn>(wrapped)));
  }

  item_type pop_sync() {
    return frq::sync_wait(pop());
  }

  static_queue queue_;
};

TEST_F(static_queue_tests, serving_leaf) {
  push_sync(1.0F, 1, 1.0F);
  auto value = pop_sync();

  EXPECT_EQ(1.0F, value);
}

TEST_F(static_queue_tests, serving_root) {
  push_sync(1.0F, 1);
  auto value = pop_sync();

  EXPECT_EQ(1.0F, value);
}

TEST_F(static_queue_tests, serving_after_release) {
  push_sync(
      [this]() -> frq::task<> {
        co_await push(3.0F, 1, 2.0F);

        co_await push(1.0F, 1, 1.0F);
        auto value1 = co_await pop();
        EXPECT_EQ(1.0F, value1);

        co_await push(4.0F, 1);
      },
      2.0F,
      1,
      2.0F);

  auto value2 = pop_sync();
  EXPECT_EQ(2.0F, value2);

  auto value3 = pop_sync();
  EXPECT_EQ(3.0F, value3);

  auto value4 = pop_sync();
  EXPECT_EQ(4.0F, value4);
}

TEST_F(static_queue_tests, serving_after_finalize) {
  push_sync(2.0F, 1, 2.0F);

  auto value2 = pop_sync([this]() -> frq::task<> {
    co_await push(3.0F, 1, 2.0F);

    co_await push(1.0F, 1, 1.0F);
    auto value1 = co_await pop();
    EXPECT_EQ(1.0F, value1);

    co_await push(4.0F, 1);
  });

  EXPECT_EQ(2.0F, value2);

  auto value3 = pop_sync();
  EXPECT_EQ(3.0F, value3);

  auto value4 = pop_sync();
  EXPECT_EQ(4.0F, value4);
}
