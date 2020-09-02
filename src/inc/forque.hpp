#pragma once

#include "key.hpp"

#include "mutex.hpp"
#include "task.hpp"

#include <list>
#include <optional>
#include <unordered_map>

namespace frq {
class element {
public:
  virtual ~element() {
  }
};

template<typename Ty, typename TKeyView, typename Alloc = std::allocator<Ty>>
class chain {
public:
  using allocator_type = Alloc;
  using value_type = Ty;
  using key_view_type = TKeyView;

public:
  task<element*> add_child(mutex_guard&& parent_guard,
                           key_view_type const& key) {
    if (key.is_last()) {
    }
    else {
    }
  }

private:
  element* parent_;
  std::list<std::unique_ptr<element>, allocator_type> children_;
};

template<typename TChain>
class fork : public element {
public:
  using parent_chain_type = TChain;
  using parent_key_view_type = typename parent_chain_type::key_view_type;

  using value_type = typename parent_chain_type::value_type;
  using allocator_type = typename parent_chain_type::allocator_type;

  using child_key_view_type = typename parent_key_view_type::next_type;
  using child_chain_type =
      chain<value_type, child_key_view_type, allocator_type>;

public:
  task<element*> add_child(mutex_guard&& parent_guard,
                           parent_key_view_type const& parent_key) {
    co_await lock_.lock();
    mutex_guard guard{lock_, std::adopt_lock};
    mutex_guard{std::move(parent_guard)};

    auto key = parent_key.next();
    auto child = children_[key];
  }

private:
  mutex lock_;

  std::unordered_map<child_key_view_type,
                     child_chain_type,
                     key_view_hash,
                     key_view_equal_to,
                     allocator_type>
      children_;
};

template<typename TChain>
class crate : public element {
public:
  using chain_type = TChain;
  using value_type = typename chain_type::value_type;

public:
private:
  std::optional<value_type> value_;
  chain_type* owner_;
};

template<typename TCrate>
class reservation {
public:
  using crate_type = TCrate;
  using value_type = typename crate_type::value_type;

private:
  crate<value_type>* crate_;
};

template<typename TCrate>
class retainment {
public:
  using crate_type = TCrate;

public:
  task<> release() {
  }

private:
  crate_type* crate_;
};
} // namespace frq
