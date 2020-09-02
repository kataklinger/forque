#pragma once

#include "sdefs.hpp"

#include <future>
#include <type_traits>
#include <utility>
#include <variant>

namespace frq {
namespace detail {
  template<typename Ty>
  struct sync_waited_traits {
    using storage_type = std::variant<std::monostate,
                                      std::exception_ptr,
                                      std::remove_reference_t<Ty>*>;
    using result_type = Ty&&;
  };

  template<>
  struct sync_waited_traits<void> {
    using storage_type = std::variant<std::monostate, std::exception_ptr>;
    using result_type = void;
  };
} // namespace detail

template<typename Ty = void>
class [[nodiscard]] sync_waited {
public:
  using value_type = Ty;

  using traits_type = detail::sync_waited_traits<Ty>;
  using storage_type = typename traits_type::storage_type;
  using result_type = typename traits_type::result_type;

  class promise_type {
  public:
    class final_awaiter {
    public:
      inline explicit final_awaiter(std::promise<void>& completion)
          : completion_{&completion} {
      }

      inline bool await_ready() noexcept {
        return false;
      }

      inline void await_suspend(
          detail::coro_handle_t<promise_type> /*unused*/) noexcept {
        completion_->set_value();
      }

      inline void await_resume() noexcept {
      }

    private:
      std::promise<void>* completion_;
    };

  public:
    inline sync_waited get_return_object() noexcept {
      return sync_waited{*this, completion_.get_future()};
    }

    std::experimental::suspend_always initial_suspend() noexcept {
      return {};
    }

    inline final_awaiter final_suspend() noexcept {
      return final_awaiter{completion_};
    }

    template<typename Tx = Ty>
    requires(
        std::same_as<std::remove_reference_t<Tx>,
                     std::remove_reference_t<value_type>>) inline final_awaiter
        yield_value(Tx&& value) noexcept {
      result_.template emplace<2>(std::addressof(value));
      return final_awaiter{completion_};
    }

    inline void return_void() noexcept {
    }

    inline void unhandled_exception() noexcept {
      result_.template emplace<1>(std::current_exception());
    }

    inline result_type result() {
      if constexpr (std::is_void_v<value_type>) {
        if (result_.index() == 1) {
          std::rethrow_exception(get<1>(result_));
        }
      }
      else {
        switch (result_.index()) {
        case 1: std::rethrow_exception(get<1>(result_));
        case 2: return static_cast<value_type&&>(*get<2>(result_));
        }
        throw std::logic_error{"invalid coroutine result"};
      }
    }

  private:
    storage_type result_;
    std::promise<void> completion_;
  };

private:
  using handle_type = detail::coro_handle_t<promise_type>;

public:
  inline explicit sync_waited(promise_type & promise,
                              std::future<void> && completion) noexcept
      : handle_{handle_type::from_promise(promise)}
      , completion_{std::move(completion)} {
  }

  inline sync_waited(sync_waited && other) noexcept
      : handle_{other.handle_}
      , completion_{std::move(other.completion_)} {
    other.handle_ = nullptr;
  }

  inline ~sync_waited() {
    if (handle_) {
      handle_.destroy();
    }
  }

  sync_waited(sync_waited const&) = delete;

  sync_waited& operator=(sync_waited const&) = delete;

  inline sync_waited& operator=(sync_waited&& rhs) noexcept {
    sync_waited{std::move(rhs)}.swap(*this);
    return *this;
  }

  inline decltype(auto) execute() {
    handle_.resume();
    completion_.wait();
    return handle_.promise().result();
  }

  inline void swap(sync_waited & other) noexcept {
    std::swap(handle_, other.handle_);
    std::swap(completion_, other.completion_);
  }

private:
  handle_type handle_;
  std::future<void> completion_;
};

template<typename Ty,
         typename Ret = decltype(std::declval<Ty&&>().await_resume())>
sync_waited<Ret> make_sync_wait(Ty&& awaitable) {
  if constexpr (!std::is_void_v<Ret>) {
    co_yield co_await std::forward<Ty>(awaitable);
  }
  else {
    co_await std::forward<Ty>(awaitable);
  }
}

template<typename Ty>
auto sync_wait(Ty&& awaitable)
    -> decltype(std::forward<Ty>(awaitable).await_resume()) {
  auto waited = make_sync_wait(std::forward<Ty>(awaitable));
  return waited.execute();
}

} // namespace frq
