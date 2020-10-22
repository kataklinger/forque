
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

frq::task<> produce(queue_type& queue, int no) {
  std::mt19937 rng(std::random_device{}());

  for (int i = 0; i < 1000; ++i) {
    auto tag = generate_tag(rng);
    auto item = co_await queue.reserve(tag);

    auto value = rng();
    printf("[producer %d] < %d\n", no, value);

    co_await item.release({tag, value});

    co_await yield();
  }
}

std::atomic<int> cnt;

frq::task<> consume(queue_type& queue, int no) {
  while (true) {
    auto item = co_await queue.get();

    printf("[consumer %d] [%d] < %d\n", no, ++cnt, item.value().value_);

    co_await item.finalize();

    co_await yield();
  }
}

struct task_entry {
public:
  inline task_entry(frq::task<>&& target)
      : target_{std::move(target)}
      , wrapped_{frq::make_sync_wait(target_)} {
  }

  inline task_entry(task_entry&& other) = delete;

  frq::task<> target_;
  frq::sync_waited<> wrapped_;
};

int main() {
  queue_type queue;

  std::list<task_entry> tasks;
  std::vector<std::thread> workers;

  tasks.emplace_back(consume(queue, 0));
  tasks.emplace_back(consume(queue, 1));
  tasks.emplace_back(produce(queue, 2));
  tasks.emplace_back(produce(queue, 3));

  for (auto& task : tasks) {
    pool_enqueue(task.wrapped_.get_handle());
  }

  for (int i = 0; i < 2; ++i) {
    workers.emplace_back(pool_loop);
  }

  for (auto& task : tasks) {
    task.wrapped_.wait();
  }

  for (auto& worker : workers) {
    worker.join();
  }
}
