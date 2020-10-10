
#include "../inc/forque.hpp"
#include "../inc/sync_wait.hpp"

#include <iostream>
#include <random>
#include <thread>
#include <vector>

using tag_type = frq::dtag<>;

struct item {
  tag_type tag_;
  unsigned int value_;
};

using reservation_type = frq::reservation<item>;
using retainment_type = frq::retainment<item>;

using runque_type = frq::make_runque_t<frq::fifo_order,
                                       frq::coro_thread_model,
                                       retainment_type,
                                       std::allocator<retainment_type>>;

using queue_type = frq::forque<item, runque_type, tag_type>;

tag_type generate_tag(std::mt19937& rng) {
  std::uniform_int_distribution<> tag_size_dist(1, 3);
  std::uniform_int_distribution<> tag_value_dist(0, 9);

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

frq::task<> produce(queue_type& queue) {
  std::mt19937 rng(std::random_device{}());

  while (true) {
    auto tag = generate_tag(rng);
    auto item = co_await queue.reserve(tag);

    co_await item.release({tag, rng()});
  }
}

frq::task<> consume(queue_type& queue) {
  while (true) {
    auto item = co_await queue.get();
    co_await item.finalize();
  }
}

int main() {
  queue_type queue;

  std::vector<std::thread> workers;

  for (size_t i = 0; i < 1; i++) {
    workers.emplace_back([&queue] { frq::sync_wait(consume(queue)); });
  }

  for (size_t i = 0; i < 2; i++) {
    workers.emplace_back([&queue] { frq::sync_wait(produce(queue)); });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  std::cout << "Hello World!\n";
}
