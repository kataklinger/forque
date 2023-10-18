#pragma once

#include "mutex.hpp"
#include "task.hpp"
#include "utility.hpp"

#include <concepts>
#include <exception>
#include <optional>
#include <variant>

#include <deque>
#include <queue>
#include <stack>

namespace frq {
template<typename Ty>
concept runnable =
    std::is_nothrow_move_constructible_v<Ty> && !std::is_reference_v<Ty>;

class interrupted : public std::exception {
public:
  const char* what() const noexcept override {
    return "runqueue stopped";
  }
};

namespace detail {
  template<typename Queue, typename Alloc>
  class base_runque_queue {
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
    explicit inline base_runque_queue(
        allocator_type const& alloc = allocator_type{}) noexcept
        : queue_{alloc} {
    }

    base_runque_queue(base_runque_queue const&) = delete;
    base_runque_queue(base_runque_queue&&) = delete;

    base_runque_queue& operator=(base_runque_queue const&) = delete;
    base_runque_queue& operator=(base_runque_queue&&) = delete;

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

template<runnable Ty,
         typename Alloc = std::allocator<Ty>,
         typename Less = std::less<Ty>>
class priority_runque_queue
    : public detail::base_runque_queue<
          std::priority_queue<Ty, std::vector<Ty, Alloc>, Less>,
          Alloc> {
private:
  using base_type = detail::base_runque_queue<
      std::priority_queue<Ty, std::vector<Ty, Alloc>, Less>,
      Alloc>;

public:
  using base_type::base_type;

  inline auto pop() noexcept {
    typename base_type::popper p{this->queue_};
    return std::move(const_cast<typename base_type::value_type&>(p.q.top()));
  }
};

template<runnable Ty, typename Alloc = std::allocator<Ty>>
class fifo_runque_queue
    : public detail::base_runque_queue<std::queue<Ty, std::deque<Ty, Alloc>>,
                                       Alloc> {
private:
  using base_type =
      detail::base_runque_queue<std::queue<Ty, std::deque<Ty, Alloc>>, Alloc>;

public:
  using base_type::base_type;

  inline auto pop() noexcept {
    typename base_type::popper p{this->queue_};
    return std::move(p.q.front());
  }
};

template<runnable Ty, typename Alloc = std::allocator<Ty>>
class lifo_runque_queue
    : public detail::base_runque_queue<std::stack<Ty, std::vector<Ty, Alloc>>,
                                       Alloc> {
private:
  using base_type =
      detail::base_runque_queue<std::stack<Ty, std::vector<Ty, Alloc>>, Alloc>;

public:
  using base_type::base_type;

  inline auto pop() noexcept {
    typename base_type::popper p{this->queue_};
    return std::move(p.q.top());
  }
};

template<typename Ty>
class runque_tratis {
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

  requires runnable<typename Ty::value_type>;

  { q.pop() }
  ->std::same_as<typename Ty::value_type>;
  {q.push(std::declval<typename Ty::value_type&&>())};
  { q.empty() }
  ->std::same_as<bool>;
};

template<typename Ty>
concept runlike = requires(Ty s) {
  typename Ty::value_type;
  requires runnable<typename Ty::value_type>;
  typename Ty::get_type;

  { s.get() }
  ->std::same_as<typename Ty::get_type>;
  {s.put(std::declval<typename Ty::value_type&&>())};
};

template<queuelike Queue, typename Mtm>
class runque;

template<queuelike Queue>
class runque<Queue, single_thread_model> {
private:
  using queue_type = Queue;

public:
  using thread_model = single_thread_model;
  using value_type = typename queue_type::value_type;
  using allocator_type = typename queue_type::allocator_type;

  using get_type = std::optional<value_type>;

public:
  inline runque(allocator_type const& alloc = allocator_type{})
      : items_{alloc} {
  }

  runque(runque const&) = delete;
  runque(runque&&) = delete;

  runque& operator=(runque const&) = delete;
  runque& operator=(runque&&) = delete;

  inline get_type get() {
    if (interrupted_) {
      throw interrupted{};
    }

    if (!items_.empty()) {
      return items_.pop();
    }

    return {};
  }

  inline void put(value_type&& value) {
    if (interrupted_) {
      throw interrupted{};
    }

    items_.push(std::move(value));
  }

  template<typename... Tys>
  requires(std::is_constructible_v<value_type, Tys...>) inline void put(
      Tys&&... args) {
    if (interrupted_) {
      throw interrupted{};
    }

    items_.push(std::forward<Tys>(args)...);
  }

  inline void interrupt() noexcept {
    interrupted_ = true;
  }

private:
  bool interrupted_{false};
  queue_type items_;
};

namespace detail {
  template<runnable Ty>
  class runque_awaitable {
  public:
    using value_type = Ty;

  public:
    inline runque_awaitable(mutex& lock) noexcept
        : guard_{lock, std::adopt_lock} {};

    inline bool await_ready() noexcept {
      return false;
    }

    inline void await_suspend(std::coroutine_handle<> waiter) noexcept {
      waiter_ = waiter;
      sink(guard_);
    }

    inline value_type& await_resume() & {
      switch (result_.index()) {
      case 1: std::rethrow_exception(std::get<1>(result_));
      case 2: return std::get<2>(result_);
      default: throw std::logic_error{"invalid coroutine result"};
      }
    }

    inline runque_awaitable* set_next(runque_awaitable* next) noexcept {
      next_ = next;
      return this;
    }

    inline runque_awaitable* get_next() const noexcept {
      return next_;
    }

    inline void resume_result(value_type&& result) {
      assert(result_.index() == 0);

      result_.template emplace<2>(std::move(result));
      waiter_.resume();
    }

    template<typename... Tys>
    inline void resume_result(Tys&&... args) {
      assert(result_.index() == 0);

      result_.template emplace<2>(std::forward<Tys>(args)...);
      waiter_.resume();
    }

    inline void resume_exception(std::exception_ptr const& exception) {
      assert(exception != nullptr);
      assert(result_.index() == 0);

      result_.template emplace<1>(std::move(exception));
      waiter_.resume();
    }

  private:
    mutex_guard guard_;

    std::variant<std::monostate, std::exception_ptr, value_type> result_;
    std::coroutine_handle<> waiter_;

    runque_awaitable* next_{nullptr};
  };

} // namespace detail

template<queuelike Queue>
class runque<Queue, coro_thread_model> {
private:
  using queue_type = Queue;

public:
  using thread_model = coro_thread_model;
  using value_type = typename queue_type::value_type;
  using allocator_type = typename queue_type::allocator_type;

  using get_type = task<value_type>;

private:
  using awaitable_type = detail::runque_awaitable<value_type>;

public:
  inline runque(allocator_type const& alloc = allocator_type{})
      : items_{alloc} {
  }

  runque(runque const&) = delete;
  runque(runque&&) = delete;

  runque& operator=(runque const&) = delete;
  runque& operator=(runque&&) = delete;

  inline get_type get() {
    co_await mutex_.lock();
    awaitable_type awaitable{mutex_};

    if (interrupted_) {
      throw interrupted{};
    }

    if (!items_.empty()) {
      co_return items_.pop();
    }

    waiters_ = awaitable.set_next(waiters_);

    co_return std::move(co_await awaitable);
  }

  inline task<> put(value_type&& value) {
    awaitable_type* awaken{nullptr};

    {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      if (interrupted_) {
        throw interrupted{};
      }

      if (waiters_ == nullptr) {
        items_.push(std::move(value));
      }
      else {
        awaken = pop_waiter();
      }
    }

    if (awaken != nullptr) {
      awaken->resume_result(std::move(value));
    }
  }

  template<typename... Tys>
  requires(std::is_constructible_v<value_type, Tys...>) inline task<> put(
      Tys&&... args) {
    awaitable_type* awaken{nullptr};

    {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      if (interrupted_) {
        throw interrupted{};
      }

      if (waiters_ == nullptr) {
        items_.push(std::forward<Tys>(args)...);
      }
      else {
        awaken = pop_waiter();
      }
    }

    if (awaken != nullptr) {
      awaken->resume_result(std::forward<Tys>(args)...);
    }
  }

  inline task<> interrupt() noexcept {
    awaitable_type* waiters{nullptr};

    {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      interrupted_ = true;
      waiters = std::exchange(waiters_, nullptr);
    }

    auto exception = std::make_exception_ptr(interrupted{});
    while (waiters != nullptr) {
      auto next = waiters->get_next();
      waiters->resume_exception(exception);
      waiters = next;
    }
  }

private:
  inline awaitable_type* pop_waiter() noexcept {
    auto awaken{waiters_};
    waiters_ = awaken->get_next();

    return awaken;
  }

private:
  awaitable_type* waiters_{nullptr};
  queue_type items_;

  bool interrupted_{false};

  mutex mutex_;
};

template<typename Order,
         typename Mtm,
         typename Ty,
         typename Alloc,
         typename... Rest>
struct make_runque;

template<typename Ty, typename Mtm, typename Alloc, typename Less>
struct make_runque<priority_order, Mtm, Ty, Alloc, Less> {
  using type = runque<priority_runque_queue<Ty, Alloc, Less>, Mtm>;
};

template<typename Ty, typename Mtm, typename Alloc>
struct make_runque<fifo_order, Mtm, Ty, Alloc> {
  using type = runque<fifo_runque_queue<Ty, Alloc>, Mtm>;
};

template<typename Ty, typename Mtm, typename Alloc>
struct make_runque<lifo_order, Mtm, Ty, Alloc> {
  using type = runque<lifo_runque_queue<Ty, Alloc>, Mtm>;
};

template<typename Order,
         typename Mtm,
         typename Ty,
         typename Alloc,
         typename... Rest>
using make_runque_t =
    typename make_runque<Order, Mtm, Ty, Alloc, Rest...>::type;

} // namespace frq
