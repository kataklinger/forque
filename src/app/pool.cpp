
#include "pool.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>

void run_queue::enqueue(coro_handle const& yielder) {
  lock_type guard{lock_};
  waiters_.push(yielder);
  if (waiters_.size() == 1) {
    cond_.notify_one();
  }
}

void run_queue::run_next() {
  lock_type guard{lock_};
  cond_.wait(guard, [this] { return !waiters_.empty(); });

  auto next = waiters_.front();
  waiters_.pop();
  guard.unlock();

  next.resume();
}

void pool::yield_awaitable::await_suspend(
    std::experimental::coroutine_handle<> const& yielder) const {
  pool_->ready_.enqueue(yielder);
}

pool::pool(std::uint32_t size) {
  workers_.reserve(size);

  for (std::uint32_t i = 0; i < size; ++i) {
    workers_.emplace_back([this] {
      while (true) {
        ready_.run_next();
      }
    });
  }
}

void pool::schedule(frq::task<>&& task) {
  auto& entry = tasks_.emplace_back(std::move(task));
  ready_.enqueue(entry.wrapped_.get_handle());
}

void pool::join() {
  for (auto& task : tasks_) {
    task.wrapped_.wait();
  }

  for (auto& worker : workers_) {
    worker.join();
  }
}
