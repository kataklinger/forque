
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

template<frq::taglike Tag, typename Tag::size_type Size>
struct dsub_tag;

template<typename Alloc, uint32_t Size>
struct dsub_tag<frq::dtag<Alloc>, Size> {
  using type = frq::dtag<Alloc>;
};

template<frq::taglike Tag, typename Tag::size_type Size>
using dsub_tag_t = typename dsub_tag<Tag, Size>::type;

template<typename Queue,
         frq::taglike Tag,
         template<frq::taglike, typename Tag::size_type>
         typename Sub>
class queue_test_impl {
public:
  using queue_type = Queue;
  using tag_type = Tag;

public:
  template<typename Fn, typename... Args>
  frq::task<> push(Fn&& wrapped,
                   item_type const& value,
                   Args&&... args) requires(wrap_callable<Fn>) {
    using sub_tag_type = Sub<tag_type, sizeof...(Args)>;

    auto reservation = co_await queue_.reserve(
        sub_tag_type{frq::construct_tag_default, std::forward<Args>(args)...});

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
    using sub_tag_type = Sub<tag_type, sizeof...(Args)>;

    co_await queue_.reserve(
        sub_tag_type{frq::construct_tag_default, std::forward<Args>(args)...},
        value);
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

private:
  queue_type queue_;
};

using static_queue_test =
    queue_test_impl<static_queue, static_tag, frq::sub_tag_t>;

using dynamic_queue_test =
    queue_test_impl<dynamic_queue, dynamic_tag, dsub_tag_t>;

class static_queue_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  static_queue_test impl_;
};

class dynamic_queue_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  dynamic_queue_test impl_;
};

template<typename Test>
void serving_leaf_impl(Test& test) {
  test.push_sync(1.0F, 1, 1.0F);
  auto value = test.pop_sync();

  EXPECT_EQ(1.0F, value);
}

TEST_F(static_queue_tests, serving_leaf) {
  serving_leaf_impl(impl_);
}

TEST_F(dynamic_queue_tests, serving_leaf) {
  serving_leaf_impl(impl_);
}

template<typename Test>
void serving_root_impl(Test& test) {
  test.push_sync(1.0F, 1);
  auto value = test.pop_sync();

  EXPECT_EQ(1.0F, value);
}

TEST_F(static_queue_tests, serving_root) {
  serving_root_impl(impl_);
}

TEST_F(dynamic_queue_tests, serving_root) {
  serving_root_impl(impl_);
}

template<typename Test>
void serving_after_release_impl(Test& test) {
  test.push_sync(
      [&test]() -> frq::task<> {
        co_await test.push(3.0F, 1, 2.0F);

        co_await test.push(1.0F, 1, 1.0F);
        auto value1 = co_await test.pop();
        EXPECT_EQ(1.0F, value1);

        co_await test.push(4.0F, 1);
      },
      2.0F,
      1,
      2.0F);

  auto value2 = test.pop_sync();
  EXPECT_EQ(2.0F, value2);

  auto value3 = test.pop_sync();
  EXPECT_EQ(3.0F, value3);

  auto value4 = test.pop_sync();
  EXPECT_EQ(4.0F, value4);
}

TEST_F(static_queue_tests, serving_after_release) {
  serving_after_release_impl(impl_);
}

TEST_F(dynamic_queue_tests, serving_after_release) {
  serving_after_release_impl(impl_);
}

template<typename Test>
void serving_after_finalize_impl(Test& test) {
  test.push_sync(2.0F, 1, 2.0F);

  auto value2 = test.pop_sync([&test]() -> frq::task<> {
    co_await test.push(3.0F, 1, 2.0F);

    co_await test.push(1.0F, 1, 1.0F);
    auto value1 = co_await test.pop();
    EXPECT_EQ(1.0F, value1);

    co_await test.push(4.0F, 1);
  });

  EXPECT_EQ(2.0F, value2);

  auto value3 = test.pop_sync();
  EXPECT_EQ(3.0F, value3);

  auto value4 = test.pop_sync();
  EXPECT_EQ(4.0F, value4);
}

TEST_F(static_queue_tests, serving_after_finalize) {
  serving_after_finalize_impl(impl_);
}

TEST_F(dynamic_queue_tests, serving_after_finalize) {
  serving_after_finalize_impl(impl_);
}
