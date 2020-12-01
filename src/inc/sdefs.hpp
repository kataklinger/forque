#pragma once

#ifdef _MSC_VER

#include <experimental/coroutine>
#define FRQ_CORO_NS std::experimental

#elif __clang__

#include <experimental/coroutine>
#define FRQ_CORO_NS std::experimental

#elif __GNUG__

#include <coroutine>
#define FRQ_CORO_NS std

#endif

namespace frq {
namespace detail {
  using coro_handle = FRQ_CORO_NS::coroutine_handle<>;

  template<typename Ty>
  using coro_handle_t = FRQ_CORO_NS::coroutine_handle<Ty>;

  using suspend_always = FRQ_CORO_NS::suspend_always;
} // namespace detail
} // namespace frq
