
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

class static_queue_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  template<typename... Args>
  frq::reservation<item_type> push(Args&&... args) {
    using tag_type = frq::sub_tag_t<static_tag, sizeof...(Args)>;
    return frq::sync_wait(queue_.reserve(
        tag_type{frq::construct_tag_default, std::forward<Args>(args)...}));
  }

  frq::retainment<item_type> pop() {
    return frq::sync_wait(queue_.get());
  }

  static_queue queue_;
};

TEST_F(static_queue_tests, get_x) {
  auto reservation = push(1, 1.0F);
  frq::sync_wait(reservation.release(1.0F));

  auto retainment = pop();
  auto value = retainment.value();
  frq::sync_wait(retainment.finalize());

  EXPECT_EQ(1.0F, value);
}
