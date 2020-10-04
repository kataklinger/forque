#pragma once

#include <memory>

namespace frq {
namespace detail {
  template<typename Alloc, typename Ty>
  struct rebind_alloc {
    using type =
        typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
  };

  template<typename Alloc, typename Ty>
  using rebind_alloc_t = typename rebind_alloc<Alloc, Ty>::type;

  template<typename Ty>
  concept sinkable =
      std::is_move_constructible_v<std::decay_t<Ty>> && !std::is_const_v<Ty>;

  template<sinkable Ty>
  void sink(Ty&& obj) noexcept(
      std::is_nothrow_move_assignable_v<std::decay_t<Ty>>) {
    std::decay_t<Ty>{std::move(obj)};
  }
} // namespace detail
} // namespace frq
