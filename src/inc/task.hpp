#pragma once

#include <atomic>
#include <concepts>
#include <coroutine>
#include <type_traits>
#include <utility>
#include <variant>

namespace frq {
namespace detail {
  template<typename Ty>
  struct task_storage_impl {
    using stored_type = Ty;

    template<typename Storage>
    static inline decltype(auto) get_value(Storage&& storage) {
      return std::get<2>(std::forward<Storage>(storage));
    }
  };

  template<typename Ty>
  struct task_storage_impl<Ty&> {
    using stored_type = Ty*;

    template<typename Storage>
    static inline decltype(auto) get_value(Storage&& storage) {
      return static_cast<Ty&>(*std::get<2>(storage));
    }
  };

  template<typename Ty>
  struct task_storage_impl<Ty&&> {
    using stored_type = Ty*;

    template<typename Storage>
    static decltype(auto) get_value(Storage&& storage) {
      return static_cast<Ty&&>(*get<2>(storage));
    }
  };

  template<typename Ty>
  struct task_traits : task_storage_impl<Ty> {
    using base_type = task_storage_impl<Ty>;

    using storage_type = std::variant<std::monostate,
                                      std::exception_ptr,
                                      typename base_type::stored_type>;

    template<typename Storage>
    static decltype(auto) get(Storage&& storage) {
      switch (storage.index()) {
      case 1: std::rethrow_exception(std::get<1>(storage));
      case 2: return base_type::get_value(std::forward<Storage>(storage));
      }
      throw std::logic_error{"invalid coroutine result"};
    }
  };

  template<>
  struct task_traits<void> : task_storage_impl<void> {
    using storage_type = std::variant<std::monostate, std::exception_ptr>;
  };
} // namespace detail

template<typename Ty = void>
class [[nodiscard]] task {
public:
  using value_type = Ty;

  using traits_type = detail::task_traits<Ty>;
  using storage_type = typename traits_type::storage_type;
  using stored_type = typename traits_type::stored_type;

  template<typename Derived>
  class promise_base {
  public:
    class final_awaiter {
    public:
      inline bool await_ready() noexcept {
        return false;
      }

      inline void
          await_suspend(std::coroutine_handle<Derived> handle) noexcept {
        if (auto& promise = handle.promise();
            promise.done_.exchange(true, std::memory_order_acq_rel)) {
          promise.continuation_.resume();
        }
      }

      inline void await_resume() noexcept {
      }
    };

  public:
    void unhandled_exception() noexcept {
      result_.template emplace<1>(std::current_exception());
    }

    inline std::suspend_always initial_suspend() noexcept {
      return {};
    }

    final_awaiter final_suspend() noexcept {
      return final_awaiter{};
    }

    inline bool set_continuation(std::coroutine_handle<> cont) noexcept {
      continuation_ = cont;
      return !done_.exchange(true, std::memory_order_acq_rel);
    }

  protected:
    storage_type result_;
    std::coroutine_handle<> continuation_;
    std::atomic<bool> done_;
  };

  class promise_void : public promise_base<promise_void> {
  public:
    using promise_base<promise_void>::promise_base;

    inline task get_return_object() noexcept {
      return task{*this};
    }

    inline void return_void() noexcept {
    }

    inline void result() {
      if (this->result_.index() == 1) {
        std::rethrow_exception(std::get<1>(this->result_));
      }
    }
  };

  class promise_typed : public promise_base<promise_typed> {
  public:
    using promise_base<promise_typed>::promise_base;

    inline task get_return_object() noexcept {
      return task{*this};
    }

    template<typename Tx>
      requires(std::convertible_to<Tx &&, value_type>)
    void return_value(Tx&& value) noexcept(
        std::is_nothrow_constructible_v<value_type, Tx&&>) {
      if constexpr (std::is_reference_v<value_type>) {
        this->result_.template emplace<2>(std::addressof(value));
      }
      else {
        this->result_.template emplace<2>(std::forward<Tx>(value));
      }
    }

    inline decltype(auto) result() & {
      return traits_type::get(this->result_);
    }

    inline decltype(auto) result() && {
      return traits_type::get(std::move(this->result_));
    }
  };

  using promise_type = std::
      conditional_t<std::is_void_v<value_type>, promise_void, promise_typed>;

private:
  using handle_type = std::coroutine_handle<promise_type>;

public:
  inline task(task&& other) noexcept
      : handle_{other.handle_} {
    other.handle_ = nullptr;
  }

  inline explicit task(promise_type& promise) noexcept
      : handle_{handle_type::from_promise(promise)} {
  }

  inline ~task() {
    if (handle_) {
      handle_.destroy();
    }
  }

  task(task const&) = delete;

  inline task& operator=(task&& rhs) {
    task{std::move(rhs)}.swap(*this);
    return *this;
  }

  task& operator=(task const&) = delete;

  inline bool await_ready() noexcept {
    return !handle_ || handle_.done();
  }

  inline bool await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_.resume();
    return handle_.promise().set_continuation(handle);
  }

  inline decltype(auto) await_resume() & {
    return handle_.promise().result();
  }

  inline decltype(auto) await_resume() && {
    return std::move(handle_.promise()).result();
  }

  inline void swap(task& other) noexcept {
    std::swap(handle_, other.handle_);
  }

  inline void start() const {
    handle_.resume();
  }

private:
  handle_type handle_;
};

} // namespace frq
