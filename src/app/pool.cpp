
#include "pool.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>

class run_queue {
private:
  using lock_type = std::unique_lock<std::mutex>;

public:
  void enqueue(coro_handle const& yielder) {
    lock_type guard{lock_};
    waiters_.push(yielder);
    if (waiters_.size() == 1) {
      cond_.notify_one();
    }
  }

  void run_next() {
    lock_type guard{lock_};
    cond_.wait(guard, [this] { return !waiters_.empty(); });

    auto next = waiters_.front();
    waiters_.pop();
    guard.unlock();

    next.resume();
  }

private:
  std::mutex lock_;
  std::queue<coro_handle> waiters_;
  std::condition_variable cond_;
};

run_queue global_run;

void yield_awaitable::await_suspend(
    std::experimental::coroutine_handle<> const& yielder) const {
  global_run.enqueue(yielder);
}

void pool_enqueue(coro_handle const& yielder) {
  global_run.enqueue(yielder);
}

void pool_loop() {
  while (true) {
    global_run.run_next();
  }
}
