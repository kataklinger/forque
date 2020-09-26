#pragma once

#include "sink.hpp"
#include "tag.hpp"

#include "mutex.hpp"
#include "task.hpp"

#include <list>
#include <optional>
#include <unordered_map>

namespace frq {

namespace detail {
  template<typename Ty>
  class item_handle {
  public:
    using value_type = Ty;

  public:
    virtual ~item_handle() {
    }

    virtual task<> release(value_type&& value) = 0;
    virtual task<> finalize() = 0;

    virtual value_type& value() noexcept = 0;
  };

  template<typename Ty>
  using item_handle_ptr = std::shared_ptr<item_handle<Ty>>;
} // namespace detail

template<typename Ty>
class retainment {
public:
  using value_type = Ty;

public:
  explicit inline retainment(
      detail::item_handle_ptr<value_type>&& handle) noexcept
      : handle_{handle} {
  }

  inline task<void> finalize() {
    return handle_->finalize();
  }

  inline value_type& value() noexcept {
    return handle_->value();
  }

private:
  detail::item_handle_ptr<value_type> handle_;
};

template<typename Ty>
class reservation {
public:
  using value_type = Ty;

public:
  explicit inline reservation(
      detail::item_handle_ptr<value_type>&& handle) noexcept
      : handle_{handle} {
  }

  inline task<> release(value_type&& value) {
    co_await handle_->release(std::move(value));
  }

private:
  detail::item_handle_ptr<value_type> handle_;
};

namespace detail {
  template<typename Ty,
           taglike LevelTag,
           sinklike Sink,
           typename Alloc = std::allocator<Ty>>
  class chain {
  public:
    using value_type = Ty;
    using level_tag_type = LevelTag;
    using sink_type = Sink;

    using allocator_type = Alloc;

  private:
    using key_type = tag_key_t<level_tag_type>;

    using next_tag_type = tag_next_t<level_tag_type>;
    using prev_tag_type = tag_prev_t<level_tag_type>;

    using next_type =
        chain<value_type, next_tag_type, sink_type, allocator_type>;
    using prev_type =
        chain<value_type, prev_tag_type, sink_type, allocator_type>;

    using sibling_type = std::optional<value_type>;
    using sibling_list =
        std::list<sibling_type,
                  detail::rebind_alloc_t<allocator_type, sibling_type>>;
    using sibling_iter = typename sibling_list::iterator;

    using children_map = std::unordered_map<
        key_type,
        next_type,
        std::hash<key_type>,
        std::equal_to<key_type>,
        detail::rebind_alloc_t<allocator_type,
                               std::pair<const key_type, next_type>>>;

    struct segment {
      inline bool forked() const noexcept {
        return !children_.empty();
      }

      sibling_list siblings_;
      children_map children_;
      std::uint64_t version_{0};
      bool active_{false};
    };

    using segment_alloc_type = detail::rebind_alloc_t<allocator_type, segment>;
    using segment_list = std::list<segment, segment_alloc_type>;
    using segment_iter = typename segment_list::iterator;

    class item_handle_impl : public item_handle<value_type> {
    public:
      inline item_handle_impl(sibling_iter position,
                              segment_iter segment,
                              chain& owner) noexcept
          : position_{position}
          , segment_{segment}
          , owner_{&owner} {
      }

      task<> release(value_type&& value) override {
        co_await owner_->release(position_, segment_, std::move(value));
      }

      task<> finalize() override {
        co_await owner_->finalize(segment_);
      }

      value_type& value() noexcept override {
        assert(*position_);
        return position_->value();
      }

    private:
      sibling_iter position_;
      segment_iter segment_;
      chain* owner_;
    };

  public:
    inline chain(sink_type& sink,
                 prev_type* parent,
                 level_tag_type&& tag,
                 allocator_type const& alloc = allocator_type{}) noexcept
        : sink_{&sink}
        , parent_{parent}
        , tag_{std::move(tag)}
        , segments_{segment_alloc_type{alloc}} {
    }

    template<viewlike View>
    task<reservation<value_type>> reserve(View view) {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      co_return co_await reserve(std::move(view), std::move(guard));
    }

  private:
    template<viewlike View>
    task<reservation<value_type>> reserve(View view, mutex_guard&& guard) {
      using view_traits = tag_view_traits<View>;

      if constexpr (view_traits::is_last) {
        co_return {add_sibling()};
      }
      else if constexpr (view_traits::is_static) {
        co_return co_await reserve_child(view.next(), std::move(guard));
      }
      else {
        if (view.last()) {
          co_return {add_sibling()};
        }
        else {
          co_return co_await reserve_child(view.next(), std::move(guard));
        }
      }
    }

  private:
    auto add_sibling() {
      using alloc_type =
          detail::rebind_alloc_t<allocator_type, item_handle_impl>;

      if (segments_.empty() || segments_.back().forked()) {
        segments_.push_back({});
      }

      auto segment = segments_.rbegin();
      ++segment->version_;

      auto& siblings = segment->siblings_;
      return make_handle(segment, siblings.insert(siblings.end(), {}));
    }

    template<viewlike View>
    task<reservation<value_type>> reserve_child(View view,
                                                mutex_guard&& guard) {
      auto& child = ensure_child(view)->second;

      co_await child.mutex_.lock();
      mutex_guard guard_child{child.mutex_, std::adopt_lock};

      mutex_guard{std::move(guard)};

      co_return child.reserve(view, std::move(guard_child));
    }

    template<viewlike View>
    auto ensure_child(View view) {
      auto& children = ensure_segment().children_;
      auto it = children.find(view.key());
      if (it == children.end()) {
        return children.insert(
            std::pair{view.key(), next_type{*sink_, this, view.sub()}});
      }

      return it;
    }

    auto& ensure_segment() {
      if (segments_.empty()) {
        segments_.push_back({});
      }

      auto& segment = segments_.back();
      ++segment.version_;

      return segment;
    }

    task<> release(sibling_iter sibling_pos,
                   segment_iter segment_pos,
                   value_type&& value) {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      assert(!*sibling_pos);
      *sibling_pos = std::move(value);

      if (segment_pos->active_ && segment_pos == begin(segments_) &&
          sibling_pos == begin(segment_pos->siblings_)) {
        co_await sink_->put(retainment{make_handle(segment_pos, sibling_pos)});
      }
    }

    task<> finalize(segment_iter segment_pos) {
      co_await parent_->mutex_.lock();
      mutex_guard guard_parent{parent_->mutex_, std::adopt_lock};

      co_await mutex_.lock();
      mutex_guard guard_owner{mutex_, std::adopt_lock};

      segment_pos->siblings_.pop_front();
      if (!segment_pos->siblings_.empty()) {
        mutex_guard{std::move(guard_parent)};

        co_await activate_sibling(segment_pos);
      }
      else if (segment_pos->forked()) {
        mutex_guard{std::move(guard_parent)};

        co_await activate_children(segment_pos);
      }
      else {
        co_await next_segment(
            segment_pos, std::move(guard_parent), std::move(guard_owner));
      }
    }

    task<> activate_sibling(segment_iter segment_pos) {
      auto sibling_pos = segment_pos->siblings_.begin();
      if (*sibling_pos) {
        co_await sink_->put(retainment{make_handle(segment_pos, sibling_pos)});
      }
    }

    task<> activate_children(segment_iter segment_pos) {
      for (auto& [_, child] : segment_pos->children_) {
        co_await child.activate_segment();
      }
    }

    task<> activate_segment(segment_iter segment_pos) {
      segment_pos->active_ = true;
      if (!segment_pos->siblings_.empty()) {
        co_await activate_sibling(segment_pos);
      }
      else {
        co_await activate_children(segment_pos);
      }
    }

    task<> activate_segment() {
      co_await mutex_.lock();
      mutex_guard guard{mutex_, std::adopt_lock};

      co_await activate_segment(segments_.begin());
    }

    task<> remove_child(next_tag_type const& tag, std::uint64_t version) {
      co_await parent_->mutex_.lock();
      mutex_guard guard_parent{parent_->mutex_, std::adopt_lock};

      co_await mutex_.lock();
      mutex_guard guard_this{mutex_, std::adopt_lock};

      auto segment_pos = segments_.begin();
      auto child = segment_pos->children_.find(tag);

      if (child != segment_pos->children_.end()) {
        co_await child->mutex_.lock();
        mutex_guard guard_child{child->mutex_, std::adopt_lock};

        if (child->get_version() == version) {
          segment_pos->children_.erase(child);
          if (segment_pos->children_.empty()) {
            mutex_guard{std::move(guard_child)};

            co_await next_segment(
                segment_pos, std::move(guard_parent), std::move(guard_this));
          }
        }
      }
    }

    task<> next_segment(segment_iter segment_pos,
                        mutex_guard&& guard_parent,
                        mutex_guard&& guard_this) {
      if (segments_.size() > 1) {
        auto next_segment = segments_.erase(segment_pos);
        mutex_guard{std::move(guard_parent)};

        co_await activate_segment(next_segment);
      }
      else {
        auto parent = parent_;

        auto version = get_version();
        auto tag = tag_;

        mutex_guard{std::move(guard_parent)};
        mutex_guard{std::move(guard_this)};

        co_await parent->remove_child(tag, version);
      }
    }

    inline std::uint64_t get_version() {
      return segments_.front().version_;
    }

    inline auto make_handle(segment_iter segment_pos,
                            sibling_iter sibling_pos) {
      using alloc_type =
          detail::rebind_alloc_t<allocator_type, item_handle_impl>;
      return std::allocate_shared<item_handle_impl>(
          alloc_type{segments_.get_allocator()},
          sibling_pos,
          segment_pos,
          *this);
    }

  private:
    mutex mutex_;

    sink_type* sink_;

    prev_type* parent_;

    level_tag_type tag_;
    segment_list segments_;
  };
} // namespace detail

template<typename Ty,
         sinklike Sink,
         taglike Tag,
         typename Alloc = std::allocator<Ty>>
class forque {
public:
  using value_type = Ty;
  using tag_type = Tag;
  using sink_type = Sink;
  using allocator_type = Alloc;

  using reservation_type = reservation<Ty>;
  using retainment_type = retainment<Ty>;

  static_assert(
      std::is_same_v<retainment_type, typename sink_type::value_type>);

private:
  using root_chain_type = detail::
      chain<value_type, tag_root_t<tag_type>, sink_type, allocator_type>;

public:
  explicit inline forque(allocator_type const& alloc = allocator_type{})
      : root_{sink_,
              nullptr,
              tag_root_t<tag_type>{frq::construct_tag_default},
              alloc} {
  }

  forque(forque&&) = delete;
  forque(forque const&) = delete;

  forque& operator=(forque&&) = delete;
  forque& operator=(forque const&) = delete;

  template<typename Tag>
  task<reservation_type> reserve(Tag const& tag) {
    return root_.reserve(view(tag));
  }

  task<retainment_type> get() noexcept {
    return sink_.get();
  }

private:
  sink_type sink_;
  root_chain_type root_;
};

} // namespace frq
