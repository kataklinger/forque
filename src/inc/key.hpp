#pragma once

#include <cassert>
#include <concepts>
#include <memory>
#include <tuple>
#include <vector>

namespace frq {

template<typename Ty>
concept static_keylike = requires(Ty k) {
  typename Ty::size_type;

  requires std::same_as<decltype(Ty::depth), typename Ty::size_type const>;
  {
    k.hash(std::integral_constant<typename Ty::size_type,
                                  typename Ty::size_type{}>{})
  }
  noexcept->std::same_as<std::size_t>;
  {
    k.equal(std::declval<Ty>(),
            std::integral_constant<typename Ty::size_type,
                                   typename Ty::size_type{}>{})
  }
  noexcept->std::convertible_to<bool>;
};

template<typename Ty>
concept dynamic_keylike = requires(Ty k) {
  typename Ty::size_type;

  {
    typename Ty::size_type {
      k.depth()
    }
  }
  noexcept;
  { k.hash(typename Ty::size_type{}) }
  noexcept->std::same_as<std::size_t>;
  { k.equal(std::declval<Ty&&>(), typename Ty::size_type{}) }
  noexcept->std::convertible_to<bool>;
};

struct construct_key_default_t {};
constexpr construct_key_default_t construct_key_default{};

struct construct_key_alloc_t {};
constexpr construct_key_alloc_t construct_key_alloc{};

struct construct_key_hash_cmp_t {};
constexpr construct_key_hash_cmp_t construct_key_hash_cmp{};

template<typename HashCmp, typename... Tys>
class static_key {
public:
  using size_type = std::uint8_t;
  using hash_compare_type = HashCmp;

  using pack_type = std::tuple<Tys...>;

  static constexpr size_type depth =
      static_cast<size_type>(std::tuple_size_v<pack_type>);

public:
  template<typename... Txs>
  constexpr inline static_key(construct_key_default_t /*unused*/, Txs&&... args)
      : hash_cmp_{}
      , values_{std::forward<Txs>(args)...} {
  }

  template<typename... Txs>
  constexpr inline static_key(construct_key_hash_cmp_t /*unused*/,
                              hash_compare_type const& hash_cmp,
                              Txs&&... args)
      : hash_cmp_{hash_cmp}
      , values_{std::forward<Txs>(args)...} {
  }

  constexpr inline static_key(
      pack_type const& values,
      hash_compare_type const& hash_cmp = hash_compare_type{})
      : hash_cmp_{hash_cmp}
      , values_{values} {
  }

  constexpr inline static_key(
      pack_type&& values,
      hash_compare_type const& hash_cmp = hash_compare_type{})
      : hash_cmp_{hash_cmp}
      , values_{std::move(values)} {
  }

  template<size_type Level>
  constexpr inline std::size_t
      hash(std::integral_constant<size_type, Level>) const noexcept {
    return hash_cmp_.hash(get<Level>(values_));
  }

  template<size_type Level>
  constexpr inline bool
      equal(static_key const& other,
            std::integral_constant<size_type, Level>) const noexcept {
    return hash_cmp_.equal_to(get<Level>(values_), get<Level>(other.values_));
  }

  pack_type const& values() const noexcept {
    return values_;
  }

private:
  [[no_unique_address]] hash_compare_type hash_cmp_;
  pack_type values_;
};

class dynamic_key_node {
public:
  virtual ~dynamic_key_node() {
  }

  virtual std::size_t hash() const noexcept = 0;
  virtual bool equal(dynamic_key_node const& other) const noexcept = 0;
};

namespace detail {
  template<typename Ty>
  class dynamic_key_node_typed : public dynamic_key_node {
  public:
    virtual Ty const& value() const = 0;
  };

  template<typename Ty, typename HashCmp>
  class dynamic_key_node_impl final : public dynamic_key_node_typed<Ty> {
  public:
    using value_type = Ty;
    using hash_compare_type = HashCmp;

  public:
    template<typename... Tys>
    explicit inline dynamic_key_node_impl(
        hash_compare_type const& hash_cmp,
        Tys&&... args) noexcept(std::is_nothrow_constructible_v<value_type,
                                                                Tys...>)
        : hash_cmp_{hash_cmp}
        , value_{std::forward<Tys>(args)...} {
    }

    std::size_t hash() const noexcept override {
      return hash_cmp_.hash(value_);
    }

    bool equal(dynamic_key_node const& other) const noexcept override {
      auto other_typed = dynamic_cast<dynamic_key_node_impl const*>(&other);
      return other_typed != nullptr &&
             hash_cmp_.equal_to(value_, other_typed->value_);
    }

    inline Ty const& value() const override {
      return value_;
    }

  private:
    [[no_unique_address]] hash_compare_type hash_cmp_;
    Ty value_;
  };

  template<typename Ty, typename HashCmp>
  using dynamic_key_node_t = dynamic_key_node_impl<std::decay_t<Ty>, HashCmp>;

  template<typename Node, typename Alloc, typename HashCmp, typename... Tys>
  inline Node* construct_dynamic_key_node(Alloc&& alloc,
                                          HashCmp const& hash_cmp,
                                          std::true_type /*unused*/,
                                          Tys&&... args) {
    auto storage{alloc.allocate(1)};
    alloc.construct(storage, std::forward<Tys>(args)..., hash_cmp);
    return storage;
  }

  template<typename Node, typename Alloc, typename HashCmp, typename... Tys>
  Node* construct_dynamic_key_node(Alloc&& alloc,
                                   HashCmp const& hash_cmp,
                                   std::false_type /*unused*/,
                                   Tys&&... args) {
    struct construct_storage {
      Alloc& alloc_;
      Node* buffer_;

      inline ~construct_storage() {
        if (buffer_ != nullptr) {
          alloc_.deallocate(buffer_, 1);
        }
      }

      inline Node* release() noexcept {
        return std::exchange(buffer_, nullptr);
      }
    };

    construct_storage storage{alloc, alloc.allocate(1)};
    std::allocator_traits<std::decay_t<Alloc>>::construct(
        alloc, storage.buffer_, hash_cmp, std::forward<Tys>(args)...);
    return storage.release();
  }

  template<typename... Tys>
  struct type_list {};

  template<typename Ty, typename Size, typename... Tys, Size... Idxs>
  auto get_dynamic_key_values(Ty const& values,
                              type_list<Tys...> /*unused,*/,
                              std::integer_sequence<Size, Idxs...> /*unused*/) {
    return std::tuple<Tys...>{
        dynamic_cast<dynamic_key_node_typed<Tys> const&>(*values[Idxs].get())
            .value()...};
  }
} // namespace detail

template<typename Alloc>
class dynamic_key_node_deleter {
public:
  using allocator_type = Alloc;
  using delete_fn = void (*)(Alloc& alloc, dynamic_key_node* ptr);

public:
  dynamic_key_node_deleter(allocator_type const& alloc, delete_fn del)
      : alloc_{alloc}
      , del_{del} {
  }

  inline void operator()(dynamic_key_node* ptr) {
    del_(alloc_, ptr);
  }

private:
  [[no_unique_address]] allocator_type alloc_;
  delete_fn del_;
};

template<typename Alloc>
using dynamic_key_node_pointer =
    std::unique_ptr<dynamic_key_node, dynamic_key_node_deleter<Alloc>>;

template<typename Ty, typename Alloc, typename HashCmp, typename... Tys>
auto make_dynamic_key_node(Alloc const& alloc,
                           HashCmp const& hash_cmp,
                           Tys&&... args) {
  using node_type = detail::dynamic_key_node_t<Ty, HashCmp>;
  using node_alloc_t =
      typename std::allocator_traits<Alloc>::template rebind_alloc<node_type>;

  auto node = detail::construct_dynamic_key_node<node_type>(
      node_alloc_t{alloc},
      hash_cmp,
      std::is_nothrow_constructible<node_type, Tys...>{},
      std::forward<Tys>(args)...);

  dynamic_key_node_deleter<Alloc> deleter{
      alloc, +[](Alloc& a, dynamic_key_node* p) {
        p->~dynamic_key_node();
        node_alloc_t{a}.deallocate(static_cast<node_type*>(p), 1);
      }};

  return dynamic_key_node_pointer<Alloc>{node, std::move(deleter)};
}

struct default_hash_compare {
  template<typename Ty>
  inline std::size_t hash(Ty const& value) const noexcept {
    return std::hash<Ty>{}(value);
  }

  template<typename Ty>
  inline bool equal_to(Ty const& left, Ty const& right) const noexcept {
    return std::equal_to<Ty>{}(left, right);
  }
};

template<typename Ty>
concept key_iterator = requires(Ty i) {
  typename std::iterator_traits<Ty>::iterator_category;
  typename std::iterator_traits<Ty>::value_type;

  requires std::same_as<std::decay_t<decltype(++i)>, Ty>;
  requires std::same_as<std::decay_t<decltype(*i)>,
                        typename std::iterator_traits<Ty>::value_type>;

  { i == i }
  ->std::convertible_to<bool>;
};

template<typename Alloc = std::allocator<dynamic_key_node>>
class dynamic_key {
public:
  using size_type = std::uint32_t;
  using allocator_type = Alloc;

  using node_pointer_type = dynamic_key_node_pointer<allocator_type>;

private:
  using storage_allocator_type = typename std::allocator_traits<
      allocator_type>::template rebind_alloc<node_pointer_type>;
  using storage_type = std::vector<node_pointer_type, storage_allocator_type>;

public:
  template<key_iterator Iter>
  inline dynamic_key(allocator_type const& alloc, Iter first, Iter last)
      : values_{std::make_move_iterator(first),
                std::make_move_iterator(last),
                alloc} {
  }

  template<key_iterator Iter>
  inline dynamic_key(Iter first, Iter last)
      : dynamic_key{allocator_type{}, first, last} {
  }

  template<typename HashCmp, typename... Tys>
  dynamic_key(allocator_type const& alloc,
              HashCmp const& hash_cmp,
              Tys&&... args)
      : values_{alloc} {
    static_assert(sizeof...(Tys) > 0);
    values_.reserve(sizeof...(Tys));

    (values_.push_back(
         make_dynamic_key_node<Tys>(alloc, hash_cmp, std::forward<Tys>(args))),
     ...);
  }

  template<typename... Tys>
  inline dynamic_key(construct_key_alloc_t /*unused*/,
                     allocator_type const& alloc,
                     Tys&&... args)
      : dynamic_key{alloc, default_hash_compare{}, std::forward<Tys>(args)...} {
  }

  template<typename HashCmp, typename... Tys>
  inline dynamic_key(construct_key_hash_cmp_t /*unused*/,
                     HashCmp const& hash_cmp,
                     Tys&&... args)
      : dynamic_key{allocator_type{}, hash_cmp, std::forward<Tys>(args)...} {
  }

  template<typename... Tys>
  dynamic_key(construct_key_default_t /*unused*/, Tys&&... args)
      : dynamic_key{allocator_type{},
                    default_hash_compare{},
                    std::forward<Tys>(args)...} {
  }

  inline size_type depth() const noexcept {
    return static_cast<size_type>(values_.size());
  }

  inline std::size_t hash(size_type level) noexcept {
    assert(level < values_.size());
    return values_[level]->hash();
  }

  inline bool equal(const dynamic_key& other, size_type level) noexcept {
    assert(level < values_.size());
    assert(level < other.values_.size());

    return values_[level]->equal(*other.values_[level]);
  }

  template<typename... Tys>
  inline auto values() const {
    assert(values_.size() == sizeof...(Tys));

    return detail::get_dynamic_key_values(
        values_,
        detail::type_list<std::decay_t<Tys>...>{},
        std::make_integer_sequence<size_type,
                                   static_cast<size_type>(sizeof...(Tys))>{});
  }

private:
  storage_type values_;
};

template<typename Level, Level Current, Level Depth>
struct static_level {
  using value_type = Level;

  static constexpr value_type current = Current;
  static constexpr value_type depth = Depth;
};

struct dynamic_level {};

template<typename Key, typename TLevel>
class key_view;

template<dynamic_keylike Key>
class key_view<Key, dynamic_level> {
public:
  using key_type = Key;
  using size_type = typename key_type::size_type;

  using pointer_type = std::shared_ptr<key_type>;

  using next_type = key_view<key_type, dynamic_level>;

public:
  explicit inline key_view()
      : key_{nullptr}
      , current_{static_cast<size_type>(0)}
      , depth_{static_cast<size_type>(0)} {
  }

  explicit inline key_view(pointer_type const& key, size_type current = 0U)
      : key_{key}
      , current_{current}
      , depth_{key->depth()} {
    assert(current_ < depth_);
  }

  inline key_view next() const noexcept {
    assert(current_ + 1 < depth_);
    return key_view{key_, current_ + 1};
  }

  inline bool is_last() const noexcept {
    return current_ + 1 == depth_;
  }

  inline size_type current() const noexcept {
    return current_;
  }

  inline size_type depth() const noexcept {
    return depth_;
  }

  inline std::size_t hash() const noexcept {
    return key_->hash(current_);
  }

  inline bool equal(key_view const& other) const noexcept {
    return current_ == other.current_ && key_->equal(*other.key_, current_);
  }

  inline void swap(key_view& other) noexcept {
    using std::swap;
    swap(key_, other.key_);
    swap(current_, other.current_);
    swap(depth_, other.depth_);
  }

  inline bool is_empty() const noexcept {
    return key_ == nullptr;
  }

private:
  pointer_type key_;
  size_type current_;
  size_type depth_;
};

template<dynamic_keylike Key>
using dynamic_key_view_t = key_view<Key, dynamic_level>;

template<typename Level, Level Current, Level Depth>
struct next_level {
  using type = std::conditional_t<(Current < Depth - 1),
                                  static_level<Level, Current + 1, Depth>,
                                  static_level<Level, Current, Depth>>;
};

template<typename Level, Level Current, Level Depth>
using next_level_t = typename next_level<Level, Current, Depth>::type;

template<static_keylike Key,
         typename Key::size_type Current,
         typename Key::size_type Depth>
class key_view<Key, static_level<typename Key::size_type, Current, Depth>> {
public:
  using key_type = Key;
  using size_type = typename key_type::size_type;

  using pointer_type = std::shared_ptr<key_type>;

  using next_type = key_view<key_type, next_level_t<size_type, Current, Depth>>;

public:
  explicit inline key_view(pointer_type const& key)
      : key_{key} {
  }

  constexpr inline next_type next() const noexcept {
    return next_type{key_};
  }

  constexpr inline bool is_last() const noexcept {
    return Current + 1 == Depth;
  }

  constexpr inline size_type current() const noexcept {
    return Current;
  }

  constexpr inline size_type depth() const noexcept {
    return Depth;
  }

  inline std::size_t hash() const noexcept {
    return key_->hash(std::integral_constant<size_type, Current>{});
  }

  inline bool equal(key_view const& other) const noexcept {
    return key_->equal(*other.key_,
                       std::integral_constant<size_type, Current>{});
  }

  inline bool is_empty() const noexcept {
    return key_ == nullptr;
  }

  inline void swap(key_view& other) noexcept {
    using std::swap;
    swap(key_, other.key_);
  }

private:
  pointer_type key_;
};

template<static_keylike Key,
         typename Key::size_type Current,
         typename Key::size_type Depth = Key::depth>
using static_key_view_t =
    key_view<Key, static_level<typename Key::size_type, Current, Depth>>;

struct key_view_hash {
  template<typename Key, typename Level>
  inline std::size_t
      operator()(key_view<Key, Level> const& view) const noexcept {
    return view.hash();
  }
};

struct key_view_equal_to {
  template<typename Key, typename Level>
  inline bool operator()(key_view<Key, Level> const& lhs,
                         key_view<Key, Level> const& rhs) const noexcept {
    return lhs.equal(rhs);
  }
};

} // namespace frq
