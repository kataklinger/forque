
#include "pool.hpp"

#include "../inc/sync_wait.hpp"

#include "../inc/forque.hpp"
#include "../inc/tag_stream.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

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

std::atomic<std::size_t> produced{0};
std::atomic<std::size_t> consumed{0};

void print_producer(char const* prefix,
                    int value,
                    tag_type const& tag,
                    int no,
                    std::size_t count) {
  std::stringstream sstream;

  sstream << prefix << "[producer " << std::setw(2) << no << "] > ["
          << std::setw(5) << count << " | " << std::setw(5) << value << "] ["
          << tag << "]" << std::endl;

  std::cout << sstream.str();
}

void print_consumer(char const* prefix,
                    retainment_type const& item,
                    int no,
                    std::size_t count) {
  std::stringstream sstream;

  sstream << prefix << "[consumer " << std::setw(2) << no << "] < ["
          << std::setw(5) << count << " | " << std::setw(5)
          << item.value().value_ << "] [" << item.value().tag_ << "]"
          << std::endl;

  std::cout << sstream.str();
}

frq::task<> produce(pool& p, queue_type& queue, int no) {
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<> item_value_dist(0, 9999);

  for (int i = 0; i < 100000; ++i) {
    auto count = ++produced;

    auto tag = generate_tag(rng);
    auto item = co_await queue.reserve(tag);

    auto value = static_cast<int>(item_value_dist(rng));

    print_producer(">>> ", value, tag, no, count);

    co_await p.yield();

    print_producer("<<< ", value, tag, no, count);

    co_await item.release({tag, value});
  }
}

frq::task<> consume(pool& p, queue_type& queue, int no) {
  while (true) {
    auto item = co_await queue.get();

    auto count = ++consumed;

    print_consumer(">>> ", item, no, count);

    co_await p.yield();

    print_consumer("<<< ", item, no, count);

    co_await item.finalize();
  }
}

int main() {
  queue_type queue;

  pool p{4};

  p.schedule(consume(p, queue, 0));
  p.schedule(consume(p, queue, 1));
  p.schedule(consume(p, queue, 3));
  p.schedule(consume(p, queue, 4));

  p.schedule(produce(p, queue, 5));
  p.schedule(produce(p, queue, 6));
  p.schedule(produce(p, queue, 7));
  p.schedule(produce(p, queue, 8));

  p.wait();

  p.stop();
}
