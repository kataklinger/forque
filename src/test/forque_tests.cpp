
#include "../inc/forque.hpp"
#include "../inc/sync_wait.hpp"

#include "gtest/gtest.h"

#include <string>

using item_type = float;
using sink_type = frq::make_sink_t<frq::fifo_order,
                                   frq::coro_thread_model,
                                   frq::retainment<item_type>,
                                   std::allocator<frq::retainment<item_type>>>;

using static_tag = frq::stag_t<int, float>;
using static_queue = frq::forque<item_type, sink_type, static_tag>;

using dynamic_tag = frq::dtag<>;
using dynamic_queue = frq::forque<item_type, sink_type, dynamic_tag>;

using reservation_type = frq::reservation<item_type>;
using retainment_type = frq::retainment<item_type>;

class static_queue_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  template<typename... Args>
  frq::task<> push(item_type value, Args&&... args) {
    using tag_type = frq::sub_tag_t<static_tag, sizeof...(Args)>;
    auto reservation = co_await queue_.reserve(
        tag_type{frq::construct_tag_default, std::forward<Args>(args)...});
    co_await reservation.release(value);
  }

  frq::task<item_type> pop() {
    auto retainment = co_await queue_.get();
    auto result = retainment.value();
    co_await retainment.finalize();
    co_return result;
  }

  static_queue queue_;
};

TEST_F(static_queue_tests, get_x) {
  frq::sync_wait(push(1.0F, 1, 1.0F));
  auto result = frq::sync_wait(pop());

  EXPECT_EQ(1.0F, result);
}
