#pragma once

#include "../inc/sync_wait.hpp"
#include "../inc/task.hpp"

#include <experimental/coroutine>
#include <list>
#include <queue>
#include <vector>

using coro_handle = std::experimental::coroutine_handle<>;

class run_queue {
private:
  using lock_type = std::unique_lock<std::mutex>;

public:
  void enqueue(coro_handle const& yielder);
  void run_next();

  void stop();

  bool is_stopped() const noexcept {
    return stopped_.load(std::memory_order_acquire);
  }

private:
  std::mutex lock_;
  std::condition_variable cond_;
  std::atomic<bool> stopped_{false};

  std::queue<coro_handle> waiters_;
};

class pool {
private:
  struct task_entry {
  public:
    inline task_entry(frq::task<>&& target)
        : target_{std::move(target)}
        , wrapped_{frq::make_sync_wait(target_)} {
    }

    task_entry(task_entry const&) = delete;
    task_entry(task_entry&& other) = delete;

    task_entry& operator=(task_entry const&) = delete;
    task_entry& operator=(task_entry&&) = delete;

    frq::task<> target_;
    frq::sync_waited<> wrapped_;
  };

public:
  class yield_awaitable {
  public:
    inline explicit yield_awaitable(pool& pool) noexcept
        : pool_{&pool} {
    }

    inline bool await_ready() const noexcept {
      return false;
    }

    void await_suspend(coro_handle const& yielder) const;

    inline void await_resume() noexcept {
    }

  private:
    pool* pool_;
  };

public:
  explicit pool(std::uint32_t size);

  pool(pool const&) = delete;
  pool(pool&&) = delete;

  pool& operator=(pool const&) = delete;
  pool& operator=(pool&&) = delete;

  void schedule(frq::task<>&& task);

  void wait();
  void stop();

  inline yield_awaitable yield() noexcept {
    return yield_awaitable{*this};
  }

private:
  std::list<task_entry> tasks_;
  std::vector<std::thread> workers_;

  run_queue ready_;
};
