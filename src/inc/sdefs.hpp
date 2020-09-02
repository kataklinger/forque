#pragma once

#include <experimental/coroutine>

namespace frq {
namespace detail {
  using coro_handle = std::experimental::coroutine_handle<>;

  template<typename Ty>
  using coro_handle_t = std::experimental::coroutine_handle<Ty>;
} // namespace detail
} // namespace frq
