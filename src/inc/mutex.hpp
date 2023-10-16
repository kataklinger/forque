#pragma once

#include <atomic>
#include <cassert>
#include <coroutine>
#include <mutex>

namespace frq {
namespace detail {

  struct mxwait {
    std::coroutine_handle<> waiter_;
    mxwait* next_{nullptr};
  };

  class mxstate {
  public:
    inline constexpr mxstate() noexcept = default;

    inline constexpr explicit mxstate(std::uintptr_t value) noexcept
        : value_{value & 3} {
      assert((value & ~3) == 0);
    }

    inline explicit mxstate(mxwait* waiter) noexcept
        : value_{reinterpret_cast<std::uintptr_t>(waiter) | 3} {
      assert((reinterpret_cast<std::uintptr_t>(waiter) & ~3) == 0);
    }

    inline constexpr bool is_free() const noexcept {
      return (value_ & 1) == 0;
    }

    inline constexpr bool has_waiters() const noexcept {
      return (value_ & 3) == 3;
    }

    inline mxwait* get_waiter() const noexcept {
      return (value_ & 3) == 3 ? reinterpret_cast<mxwait*>(value_ & ~3)
                               : nullptr;
    }

    inline mxstate& operator=(mxwait* waiter) noexcept {
      assert((reinterpret_cast<std::uintptr_t>(waiter) & 3) == 0);
      value_ = reinterpret_cast<std::uintptr_t>(waiter) | 3;
      return *this;
    }

  private:
    std::uintptr_t value_{0};
  };

  class mximpl {
  private:
    static constexpr mxstate free_state{};
    static constexpr mxstate taken_state{1};

  public:
    bool lock(mxwait& entry) noexcept;
    bool try_lock() noexcept;
    void release();

  private:
    std::atomic<mxstate> state_;
    mxwait* waiters_{nullptr};
  };
} // namespace detail

class mutex_guard;

class mutex {
  friend mutex_guard;

public:
  class awaitable {
  public:
    inline explicit awaitable(detail::mximpl& impl) noexcept
        : impl_{&impl}
        , entry_{} {
    }

    inline bool await_ready() noexcept {
      return false;
    }

    inline bool await_suspend(std::coroutine_handle<> handle) noexcept {
      entry_.waiter_ = handle;
      return !impl_->lock(entry_);
    }

    inline void await_resume() noexcept {
    }

  protected:
    detail::mximpl* impl_;

  private:
    detail::mxwait entry_;
  };

  class scoped_awaitable : public awaitable {
  public:
    using awaitable::awaitable;

    [[nodiscard]] mutex_guard await_resume() noexcept;
  };

public:
  mutex() = default;

  mutex(mutex const&) = delete;
  mutex(mutex&&) = delete;

  mutex& operator=(mutex const&) = delete;
  mutex& operator=(mutex&&) = delete;

  ~mutex() = default;

  [[nodiscard]] inline awaitable lock() noexcept {
    return awaitable{impl_};
  }

  [[nodiscard]] inline scoped_awaitable scope_lock() noexcept {
    return scoped_awaitable{impl_};
  }

  [[nodiscard]] inline bool try_lock() noexcept {
    return impl_.try_lock();
  }

  inline void unlock() {
    impl_.release();
  }

private:
  detail::mximpl impl_;
};

class mutex_guard {
  friend mutex::scoped_awaitable;

public:
  inline mutex_guard() noexcept
      : lock_{nullptr} {
  }

  inline mutex_guard(mutex& lock, std::adopt_lock_t /*unused*/) noexcept
      : mutex_guard{lock.impl_} {
  }

  inline mutex_guard(mutex_guard&& other) noexcept
      : lock_{other.lock_} {
    other.lock_ = nullptr;
  }

  inline ~mutex_guard() {
    if (lock_ != nullptr) {
      lock_->release();
    }
  }

  mutex_guard(mutex_guard const&) = delete;

  mutex_guard& operator=(mutex_guard const&) = delete;
  mutex_guard& operator=(mutex_guard&&) = delete;

private:
  inline explicit mutex_guard(detail::mximpl& lock) noexcept
      : lock_{&lock} {
  }

private:
  detail::mximpl* lock_;
};

} // namespace frq
