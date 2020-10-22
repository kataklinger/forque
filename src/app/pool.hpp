#pragma once

#include <experimental/coroutine>

using coro_handle = std::experimental::coroutine_handle<>;

class yield_awaitable {
public:
  inline bool await_ready() const noexcept {
    return false;
  }

  void await_suspend(coro_handle const& yielder) const;

  inline void await_resume() noexcept {
  }
};

inline yield_awaitable yield() noexcept {
  return yield_awaitable{};
}

void pool_enqueue(coro_handle const& yielder);

void pool_loop();
