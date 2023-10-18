
#include "mutex.hpp"

bool frq::detail::mximpl::lock(mxwait& entry) noexcept {
  auto expected{free_state};
  auto desired{taken_state};

  while (!state_.compare_exchange_weak(expected,
                                       desired,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
    if (!expected.is_free()) {
      entry.next_ = expected.get_waiter();
      desired = &entry;
    }
    else {
      desired = taken_state;
    }
  }

  return expected.is_free();
}

bool frq::detail::mximpl::try_lock() noexcept {
  auto expected{free_state};
  while (!state_.compare_exchange_weak(expected,
                                       taken_state,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
    if (!expected.is_free()) {
      return false;
    }
  }

  return true;
}

void frq::detail::mximpl::release() {
  assert(!state_.load(std::memory_order_relaxed).is_free());

  auto* head = waiters_;
  if (head == nullptr) {
    auto expected{taken_state};

    if (state_.compare_exchange_strong(expected,
                                       free_state,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
      return;
    }

    expected = state_.exchange(taken_state, std::memory_order_acquire);

    auto* cur = expected.get_waiter();
    do {
      auto* next = cur->next_;
      cur->next_ = head;
      head = cur;
      cur = next;
    } while (cur != nullptr);
  }

  waiters_ = head->next_;
  head->waiter_.resume();
}

frq::mutex_guard frq::mutex::scoped_awaitable::await_resume() noexcept {
  return mutex_guard{*impl_};
}
