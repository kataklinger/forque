
#include "pool.hpp"

#include "../inc/forque.hpp"
#include "../inc/sync_wait.hpp"

#include <iostream>
#include <random>
#include <thread>
#include <vector>

using tag_type = frq::dtag<>;

struct item {
  tag_type tag_;
  int value_;
};

using reservation_type = frq::reservation<item>;
using retainment_type = frq::retainment<item>;

using runque_type = frq::make_runque_t<frq::fifo_order,
                                       frq::coro_thread_model,
                                       retainment_type,
                                       std::allocator<retainment_type>>;

using queue_type = frq::forque<item, runque_type, tag_type>;

tag_type generate_tag(std::mt19937& rng) {
  std::uniform_int_distribution<> tag_size_dist(1, 5);
  std::uniform_int_distribution<> tag_value_dist(0, 2);

  std::vector<frq::dtag_value> tag_values;

  auto tag_size = tag_size_dist(rng);
  while (tag_size-- > 0) {
    auto node = frq::make_dtag_node<int>(std::allocator<frq::dtag_node>{},
                                         frq::default_hash_compare{},
                                         tag_value_dist(rng));

    tag_values.push_back(std::move(node));
  }

  return tag_type{begin(tag_values), end(tag_values)};
}

frq::task<> produce(pool& p, queue_type& queue, int no) {
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<> item_value_dist(1000, 9999);

  for (int i = 0; i < 1000; ++i) {
    auto tag = generate_tag(rng);
    auto item = co_await queue.reserve(tag);

    auto value = static_cast<int>(item_value_dist(rng));
    printf("[producer %d] < %d\n", no, value);

    co_await item.release({tag, value});

    co_await p.yield();
  }
}

std::atomic<int> consumed{0};

frq::task<> consume(pool& p, queue_type& queue, int no) {
  while (true) {
    auto item = co_await queue.get();

    printf("[consumer %d] [%d] < %d\n", no, ++consumed, item.value().value_);

    co_await item.finalize();

    co_await p.yield();
  }
}

int main() {
  queue_type queue;

  pool p{4};

  p.schedule(consume(p, queue, 0));
  p.schedule(consume(p, queue, 1));
  p.schedule(produce(p, queue, 2));
  p.schedule(produce(p, queue, 3));

  p.wait();

  p.stop();
}
