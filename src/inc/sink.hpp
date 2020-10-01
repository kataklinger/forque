#pragma once

#include "mutex.hpp"
#include "task.hpp"

#include <concepts>
#include <optional>

#include <deque>
#include <queue>
#include <stack>

namespace frq {
template<typename Ty>
concept sinkable = std::is_nothrow_move_constructible_v<Ty>;

namespace detail {
  template<typename Queue, typename Alloc>
  class base_sink_queue {
  public:
    using queue_type = Queue;
    using allocator_type = Alloc;

    using value_type = typename queue_type::value_type;

  protected:
    struct popper {
      inline ~popper() {
        q.pop();
      }
      queue_type& q;
    };

  public:
    explicit inline base_sink_queue(
        allocator_type const& alloc = allocator_type{}) noexcept
        : queue_{alloc} {
    }

    base_sink_queue(base_sink_queue const&) = delete;
    base_sink_queue(base_sink_queue&&) = delete;

    base_sink_queue& operator=(base_sink_queue const&) = delete;
    base_sink_queue& operator=(base_sink_queue&&) = delete;

    inline void push(value_type&& value) {
      queue_.push(std::move(value));
    }

    template<typename... Tys>
    requires(std::is_constructible_v<value_type, Tys...>) inline void push(
        Tys&&... args) {
      queue_.emplace(std::forward<Tys>(args)...);
    }

    inline bool empty() const noexcept {
      return queue_.empty();
    }

  protected:
    queue_type queue_;
  };
} // namespace detail

template<sinkable Ty,
         typename Alloc = std::allocator<Ty>,
         typename Less = std::less<Ty>>
class priority_sink_queue
    : public detail::base_sink_queue<
          std::priority_queue<Ty, std::vector<Ty, Alloc>, Less>,
          Alloc> {
private:
  using base_type = detail::base_sink_queue<
      std::priority_queue<Ty, std::vector<Ty, Alloc>, Less>,
      Alloc>;

public:
  using base_type::base_type;

  inline auto pop() noexcept {
    typename base_type::popper p{this->queue_};
    return std::move(const_cast<typename base_type::value_type&>(p.q.top()));
  }
};

template<sinkable Ty, typename Alloc = std::allocator<Ty>>
class fifo_sink_queue
    : public detail::base_sink_queue<std::queue<Ty, std::deque<Ty, Alloc>>,
                                     Alloc> {
private:
  using base_type =
      detail::base_sink_queue<std::queue<Ty, std::deque<Ty, Alloc>>, Alloc>;

public:
  using base_type::base_type;

  inline auto pop() noexcept {
    typename base_type::popper p{this->queue_};
    return std::move(p.q.front());
  }
};

template<sinkable Ty, typename Alloc = std::allocator<Ty>>
class lifo_sink_queue
    : public detail::base_sink_queue<std::stack<Ty, std::vector<Ty, Alloc>>,
                                     Alloc> {
private:
  using base_type =
      detail::base_sink_queue<std::stack<Ty, std::vector<Ty, Alloc>>, Alloc>;

public:
  using base_type::base_type;

  inline auto pop() noexcept {
    typename base_type::popper p{this->queue_};
    return std::move(p.q.top());
  }
};

template<typename Ty>
class sink_tratis {
  using port_type = Ty;

  using value_model = typename port_type::value_model;
  using thread_model = typename port_type::thread_model;
  using return_type = std::decay_t<decltype(std::declval<port_type>().get())>;
};

struct priority_order {};
struct fifo_order {};
struct lifo_order {};

struct single_thread_model {};
struct multi_thread_model {};
struct coro_thread_model {};

template<typename Ty>
concept queuelike = requires(Ty q) {
  typename Ty::value_type;
  typename Ty::allocator_type;

  requires sinkable<typename Ty::value_type>;

  { q.pop() }
  ->std::same_as<typename Ty::value_type>;
  {q.push(std::declval<typename Ty::value_type&&>())};
  { q.empty() }
  ->std::same_as<bool>;
};

template<typename Ty>
concept sinklike = requires(Ty s) {
  typename Ty::value_type;
  requires sinkable<typename Ty::value_type>;
  typename Ty::get_type;

  { s.get() }
  ->std::same_as<typename Ty::get_type>;
  {s.put(std::declval<typename Ty::value_type&&>())};
};

template<queuelike Queue, typename Mtm>
class sink;

template<queuelike Queue>
class sink<Queue, single_thread_model> {
private:
  using queue_type = Queue;

public:
  using thread_model = single_thread_model;
  using value_type = typename queue_type::value_type;
  using allocator_type = typename queue_type::allocator_type;

  using get_type = std::optional<value_type>;

public:
  inline sink(allocator_type const& alloc = allocator_type{})
      : items_{alloc} {
  }

  sink(sink const&) = delete;
  sink(sink&&) = delete;

  sink& operator=(sink const&) = delete;
  sink& operator=(sink&&) = delete;

  inline get_type get() noexcept {
    if (!items_.empty()) {
      return items_.pop();
    }

    return {};
  }

  inline void put(value_type&& value) {
    items_.push(std::move(value));
  }

  template<typename... Tys>
  requires(std::is_constructible_v<value_type, Tys...>) inline void put(
      Tys&&... args) {
    items_.push(std::forward<Tys>(args)...);
  }

private:
  queue_type items_;
};

namespace detail {
  template<sinkable Ty>
  class sink_awaitable {
  public:
    using value_type = Ty;

  public:
    inline sink_awaitable() noexcept = default;
    explicit inline sink_awaitable(value_type&& result)
        : result_{result} {
    }

    inline bool await_ready() noexcept {
      return false;
    }

    inline void await_suspend(coro_handle waiter) noexcept {
      waiter_ = waiter;
    }

    inline value_type await_resume() & noexcept(
        std::is_nothrow_copy_constructible_v<value_type>) {
      assert(!!result_);
      return std::move(*result_);
    }

    inline value_type await_resume() && noexcept {
      assert(!!result_);
      return *result_;
    }

    inline void set_next(sink_awaitable* next) noexcept {
      next_ = next;
    }

    inline sink_awaitable* get_next() const noexcept {
      return next_;
    }

    inline void resume(value_type&& result) {
      assert(!result_);
      result_ = std::move(result);
      waiter_.resume();
    }

    template<typename... Tys>
    inline void resume(Tys&&... args) {
      assert(!result_);
      result_.emplace(std::forward<Tys>(args)...);
      waiter_.resume();
    }

  private:
    std::optional<value_type> result_;
    coro_handle waiter_;

    sink_awaitable* next_{nullptr};
  };

} // namespace detail

template<queuelike Queue>
class sink<Queue, coro_thread_model> {
private:
  using queue_type = Queue;

public:
  using thread_model = coro_thread_model;
  using value_type = typename queue_type::value_type;
  using allocator_type = typename queue_type::allocator_type;

  using get_type = task<value_type>;

private:
  using awaitable_type = detail::sink_awaitable<value_type>;

public:
  inline sink(allocator_type const& alloc = allocator_type{})
      : items_{alloc} {
  }

  sink(sink const&) = delete;
  sink(sink&&) = delete;

  sink& operator=(sink const&) = delete;
  sink& operator=(sink&&) = delete;

  inline get_type get() noexcept {
    detail::sink_awaitable<value_type> awaitable{};

    {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      if (!items_.empty()) {
        co_return items_.pop();
      }

      push_waiter(awaitable);
    }

    co_return co_await awaitable;
  }

  inline task<> put(value_type&& value) {
    awaitable_type* awaken{nullptr};

    {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      if (waiters_ == nullptr) {
        items_.push(std::move(value));
      }
      else {
        awaken = pop_waiter();
      }
    }

    if (awaken != nullptr) {
      awaken->resume(std::move(value));
    }
  }

  template<typename... Tys>
  requires(std::is_constructible_v<value_type, Tys...>) inline task<> put(
      Tys&&... args) {
    awaitable_type* awaken{nullptr};

    {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      if (waiters_ == nullptr) {
        items_.push(std::forward<Tys>(args)...);
      }
      else {
        awaken = pop_waiter();
      }
    }

    if (awaken != nullptr) {
      awaken->resume(std::forward<Tys>(args)...);
    }
  }

private:
  inline void push_waiter(awaitable_type& waiter) noexcept {
    waiter.set_next(waiters_);
    waiters_ = &waiter;
  }

  inline awaitable_type* pop_waiter() noexcept {
    auto awaken{waiters_};
    waiters_ = awaken->get_next();

    return awaken;
  }

private:
  awaitable_type* waiters_{nullptr};
  queue_type items_;

  mutex mutex_;
};

template<typename Order,
         typename Mtm,
         typename Ty,
         typename Alloc,
         typename... Rest>
struct make_sink;

template<typename Ty, typename Mtm, typename Alloc, typename Less>
struct make_sink<priority_order, Mtm, Ty, Alloc, Less> {
  using type = sink<priority_sink_queue<Ty, Alloc, Less>, Mtm>;
};

template<typename Ty, typename Mtm, typename Alloc>
struct make_sink<fifo_order, Mtm, Ty, Alloc> {
  using type = sink<fifo_sink_queue<Ty, Alloc>, Mtm>;
};

template<typename Ty, typename Mtm, typename Alloc>
struct make_sink<lifo_order, Mtm, Ty, Alloc> {
  using type = sink<lifo_sink_queue<Ty, Alloc>, Mtm>;
};

template<typename Order,
         typename Mtm,
         typename Ty,
         typename Alloc,
         typename... Rest>
using make_sink_t = typename make_sink<Order, Mtm, Ty, Alloc, Rest...>::type;

} // namespace frq
