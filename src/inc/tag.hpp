#pragma once

#include "utility.hpp"

#include <cassert>
#include <concepts>
#include <memory>
#include <tuple>
#include <vector>

namespace frq {

struct construct_tag_default_t {};
constexpr construct_tag_default_t construct_tag_default{};

struct construct_tag_alloc_t {};
constexpr construct_tag_alloc_t construct_tag_alloc{};

struct construct_tag_hash_cmp_t {};
constexpr construct_tag_hash_cmp_t construct_tag_hash_cmp{};

namespace detail {
  template<typename Tuple, typename Seq>
  struct stag_helper;

  template<typename Tuple, std::uint8_t... Idxs>
  struct stag_helper<Tuple, std::integer_sequence<std::uint8_t, Idxs...>> {
    using type = std::tuple<std::tuple_element_t<Idxs, Tuple>...>;
  };

  template<typename Tuple, typename Seq>
  using stag_helper_t = typename stag_helper<Tuple, Seq>::type;
} // namespace detail

template<typename Ty>
concept taglike = requires(Ty t) {
  typename Ty::size_type;
  typename Ty::storage_type;

  {std::size_t{typename Ty::size_type{}}};

  { t.values() }
  noexcept->std::convertible_to<typename Ty::storage_type>;
};

template<std::uint8_t Size, typename... Tys>
class stag {
public:
  static_assert(Size <= sizeof...(Tys));

  using size_type = std::uint8_t;
  using storage_type =
      detail::stag_helper_t<std::tuple<Tys...>,
                            std::make_integer_sequence<size_type, Size>>;

  static constexpr size_type size = Size;

public:
  template<typename... Txs>
  constexpr inline stag(construct_tag_default_t /*unused*/, Txs&&... args)
      : values_{std::forward<Txs>(args)...} {
  }

  constexpr inline stag(storage_type const& values)
      : values_{values} {
  }

  constexpr inline stag(storage_type&& values)
      : values_{std::move(values)} {
  }

  inline storage_type const& values() const noexcept {
    return values_;
  }

  inline auto key() const noexcept {
    return get<size - 1>(values_);
  }

private:
  storage_type values_;
};

template<typename... Tys>
using stag_t = stag<static_cast<std::uint8_t>(sizeof...(Tys)), Tys...>;

class dtag_node {
public:
  virtual ~dtag_node() {
  }

  virtual std::size_t hash() const noexcept = 0;
  virtual bool equal(dtag_node const& other) const noexcept = 0;
};

using dtag_node_ptr = std::shared_ptr<dtag_node>;

namespace detail {
  template<typename Ty>
  class dtag_node_typed : public dtag_node {
  public:
    virtual Ty const& value() const = 0;
  };

  template<typename Ty, typename HashCmp>
  class dtag_node_impl final : public dtag_node_typed<Ty> {
  public:
    using value_type = Ty;
    using hash_compare_type = HashCmp;

  public:
    template<typename... Tys>
    explicit inline dtag_node_impl(
        hash_compare_type const& hash_cmp,
        Tys&&... args) noexcept(std::is_nothrow_constructible_v<value_type,
                                                                Tys...>)
        : hash_cmp_{hash_cmp}
        , value_{std::forward<Tys>(args)...} {
    }

    std::size_t hash() const noexcept override {
      return hash_cmp_.hash(value_);
    }

    bool equal(dtag_node const& other) const noexcept override {
      auto other_typed = dynamic_cast<dtag_node_impl const*>(&other);
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
  using dtag_node_t = dtag_node_impl<std::decay_t<Ty>, HashCmp>;

  template<typename... Tys>
  struct type_list {};

  template<typename Ty, typename Size, typename... Tys, Size... Idxs>
  auto get_dtag_values(Ty const& values,
                       type_list<Tys...> /*unused,*/,
                       std::integer_sequence<Size, Idxs...> /*unused*/) {
    return std::tuple<Tys...>{
        dynamic_cast<dtag_node_typed<Tys> const&>(values[Idxs].node())
            .value()...};
  }
} // namespace detail

class dtag_value {
public:
  explicit inline dtag_value(dtag_node_ptr const& tag_node)
      : tag_node_{tag_node} {
  }

  explicit inline dtag_value(dtag_node_ptr&& tag_node)
      : tag_node_{std::move(tag_node)} {
  }

  inline dtag_node_ptr::element_type const& node() const {
    return *tag_node_;
  }

  inline std::size_t hash() const noexcept {
    return tag_node_->hash();
  }

  inline bool operator==(dtag_value const& rhs) const noexcept {
    return tag_node_->equal(*rhs.tag_node_);
  }

  inline bool operator!=(dtag_value const& rhs) const noexcept {
    return !(*this == rhs);
  }

private:
  dtag_node_ptr tag_node_;
};

template<typename Ty, typename Alloc, typename HashCmp, typename... Tys>
dtag_value
    make_dtag_node(Alloc const& alloc, HashCmp const& hash_cmp, Tys&&... args) {
  using node_type = detail::dtag_node_t<Ty, HashCmp>;
  return dtag_value{std::allocate_shared<node_type>(
      alloc, hash_cmp, std::forward<Tys>(args)...)};
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
concept tag_iterator = requires(Ty i) {
  typename std::iterator_traits<Ty>::iterator_category;
  typename std::iterator_traits<Ty>::value_type;

  requires std::same_as<std::decay_t<decltype(++i)>, Ty>;
  requires std::same_as<std::decay_t<decltype(*i)>,
                        typename std::iterator_traits<Ty>::value_type>;

  { i == i }
  ->std::convertible_to<bool>;
};

template<typename Alloc = std::allocator<dtag_node>>
class dtag {
public:
  using size_type = std::uint32_t;
  using allocator_type = Alloc;
  using storage_type =
      std::vector<dtag_value,
                  detail::rebind_alloc_t<allocator_type, dtag_value>>;

public:
  template<tag_iterator Iter>
  inline dtag(allocator_type const& alloc, Iter first, Iter last)
      : values_{std::make_move_iterator(first),
                std::make_move_iterator(last),
                alloc} {
  }

  template<tag_iterator Iter>
  inline dtag(Iter first, Iter last)
      : dtag{allocator_type{}, first, last} {
  }

  template<typename HashCmp, typename... Tys>
  dtag(allocator_type const& alloc, HashCmp const& hash_cmp, Tys&&... args)
      : values_{alloc} {
    static_assert(sizeof...(Tys) > 0);
    values_.reserve(sizeof...(Tys));

    (values_.push_back(
         make_dtag_node<Tys>(alloc, hash_cmp, std::forward<Tys>(args))),
     ...);
  }

  template<typename... Tys>
  inline dtag(construct_tag_alloc_t /*unused*/,
              allocator_type const& alloc,
              Tys&&... args)
      : dtag{alloc, default_hash_compare{}, std::forward<Tys>(args)...} {
  }

  template<typename HashCmp, typename... Tys>
  inline dtag(construct_tag_hash_cmp_t /*unused*/,
              HashCmp const& hash_cmp,
              Tys&&... args)
      : dtag{allocator_type{}, hash_cmp, std::forward<Tys>(args)...} {
  }

  template<typename... Tys>
  dtag(construct_tag_default_t /*unused*/, Tys&&... args)
      : dtag{allocator_type{},
             default_hash_compare{},
             std::forward<Tys>(args)...} {
  }

  inline storage_type const& values() const noexcept {
    return values_;
  }

  template<typename... Tys>
  inline auto pack() const {
    assert(values_.size() == sizeof...(Tys));

    return detail::get_dtag_values(
        values_,
        detail::type_list<std::decay_t<Tys>...>{},
        std::make_integer_sequence<size_type,
                                   static_cast<size_type>(sizeof...(Tys))>{});
  }

  inline auto key() const noexcept {
    return values_.back();
  }

private:
  storage_type values_;
};

struct etag_value {};

template<typename Tag>
struct tag_root {
  using type = Tag;
};

template<std::uint8_t Size, typename... Tys>
struct tag_root<stag<Size, Tys...>> {
  using type = stag<0, Tys...>;
};

template<typename Tag>
using tag_root_t = typename tag_root<Tag>::type;

template<typename Tag>
struct tag_next {
  using type = Tag;
};

template<std::uint8_t Size, typename... Tys>
struct tag_next<stag<Size, Tys...>> {
  using type = std::conditional_t<(Size < (sizeof...(Tys))),
                                  stag<Size + 1, Tys...>,
                                  stag<0, Tys...>>;
};

template<typename Tag>
using tag_next_t = typename tag_next<Tag>::type;

template<typename Tag>
struct tag_prev {
  using type = Tag;
};

template<std::uint8_t Size, typename... Tys>
struct tag_prev<stag<Size, Tys...>> {
  using type =
      std::conditional_t<(Size > 0), stag<Size - 1, Tys...>, stag<0, Tys...>>;
};

template<typename Tag>
using tag_prev_t = typename tag_prev<Tag>::type;

template<typename Tag>
struct tag_key;

struct etag_value_helper {
  using type = etag_value;
};

template<std::uint8_t Size, typename... Tys>
struct tag_key<stag<Size, Tys...>> {
  static_assert(Size <= sizeof...(Tys));
  using type = typename std::conditional_t<
      (Size > 0),
      std::tuple_element<Size - 1, std::tuple<Tys...>>,
      etag_value_helper>::type;
};

template<typename Alloc>
struct tag_key<dtag<Alloc>> {
  using type = dtag_value;
};

template<typename Tag>
using tag_key_t = typename tag_key<Tag>::type;

namespace detail {
  template<taglike Tag, typename Tag::size_type Sub>
  struct sub_tag;

  template<std::uint8_t Size, std::uint8_t Sub, typename... Tys>
  struct sub_tag<stag<Size, Tys...>, Sub> {
    using type = stag<Sub, Tys...>;
  };

  template<taglike Tag, typename Tag::size_type Sub>
  using sub_tag_t = typename sub_tag<Tag, Sub>::type;

  template<std::uint8_t Sub,
           std::uint8_t Size,
           typename... Tys,
           std::uint8_t... Idxs>
  inline auto
      sub_helper(stag<Size, Tys...> const& tag,
                 std::integer_sequence<std::uint8_t, Idxs...> /*unused*/) {
    using sub_type = sub_tag_t<stag<Size, Tys...>, Sub>;
    auto& values = tag.values();
    return sub_type{construct_tag_default, std::get<Idxs>(values)...};
  }

  template<std::uint8_t Sub, std::uint8_t Size, typename... Tys>
  auto sub(stag<Size, Tys...> const& tag) {
    static_assert(Sub <= Size);
    return sub_helper<Sub>(tag,
                           std::make_integer_sequence<std::uint8_t, Sub>{});
  }

  template<typename Tag>
  struct is_tag_nothrow_copyable : std::false_type {};

  template<std::uint8_t Size, typename... Tys>
  struct is_tag_nothrow_copyable<stag<Size, Tys...>>
      : std::conjunction<std::is_nothrow_copy_constructible<Tys>...> {};

  template<typename Tag>
  constexpr bool is_tag_nothrow_copyable_v =
      is_tag_nothrow_copyable<Tag>::value;
} // namespace detail

template<typename Ty>
concept viewlike = requires(Ty v) {
  typename Ty::tag_type;
  typename Ty::key_type;
  typename Ty::sub_type;
  typename Ty::next_type;

  { v.key() }
  ->std::same_as<typename Ty::key_type>;

  { v.sub() }
  ->std::same_as<typename Ty::sub_type>;

  { v.next() }
  noexcept->std::same_as<typename Ty::next_type>;

  { v.last() }
  noexcept->std::convertible_to<bool>;
};

template<taglike Tag, typename Tag::size_type Level>
class stag_view {
public:
  using tag_type = Tag;
  using key_type = std::tuple_element_t<Level, typename tag_type::storage_type>;
  using sub_type = detail::sub_tag_t<tag_type, Level + 1>;
  using next_type = std::conditional_t<(Level < Tag::size - 1),
                                       stag_view<tag_type, Level + 1>,
                                       stag_view<tag_type, Level>>;

public:
  stag_view(tag_type const& tag)
      : tag_{&tag} {
  }

  inline key_type key() const noexcept(std::is_copy_constructible_v<key_type>) {
    return get<Level>(tag_->values());
  }

  inline sub_type sub() const
      noexcept(detail::is_tag_nothrow_copyable_v<sub_type>) {
    return detail::sub<Level + 1>(*tag_);
  }

  inline next_type next() const noexcept {
    return next_type{*tag_};
  }

  inline bool last() const noexcept {
    return Level == tag_type::size - 1;
  }

private:
  tag_type const* tag_;
};

template<typename Tag>
class dtag_view {
public:
  using tag_type = Tag;
  using key_type = dtag_value;
  using sub_type = tag_type;
  using next_type = dtag_view<tag_type>;
  using size_type = typename tag_type::size_type;

public:
  dtag_view(tag_type const& tag, size_type level)
      : tag_{&tag}
      , level_{level} {
  }

  inline key_type key() const noexcept {
    return tag_->values()[level_];
  }

  inline sub_type sub() const {
    using alloc_type = typename sub_type::allocator_type;

    auto& values = tag_->values();
    return sub_type{alloc_type{values.get_allocator()},
                    values.begin(),
                    values.begin() + (level_ + 1)};
  }

  inline next_type next() const noexcept {
    return next_type{*tag_, last() ? level_ : level_ + 1};
  }

  inline bool last() const noexcept {
    return level_ == tag_->values().size() - 1;
  }

private:
  tag_type const* tag_;
  size_type level_;
};

template<typename View>
struct tag_traits {
  static constexpr bool is_static = false;
  static constexpr bool is_root = false;
  static constexpr bool is_last = false;
};

template<std::uint8_t Size, typename... Tys>
struct tag_traits<stag<Size, Tys...>> {
  static constexpr bool is_static = true;
  static constexpr bool is_root = Size == 0;
  static constexpr bool is_last = Size == sizeof...(Tys);
};

template<typename View>
struct tag_view_traits {
  static constexpr bool is_static = false;
  static constexpr bool is_last = false;
};

template<std::uint8_t Size, std::uint8_t Level, typename... Tys>
struct tag_view_traits<stag_view<stag<Size, Tys...>, Level>> {
  static constexpr bool is_static = true;
  static constexpr bool is_last = Level == Size - 1;
};

template<std::uint8_t Size, typename... Tys>
inline auto view(stag<Size, Tys...> const& tag) noexcept {
  return stag_view<stag<Size, Tys...>, 0>{tag};
}

template<typename Alloc>
inline auto view(dtag<Alloc> const& tag) noexcept {
  return dtag_view<dtag<Alloc>>{tag, 0};
}
} // namespace frq

namespace std {
template<>
struct hash<frq::dtag_value> {
  inline size_t operator()(frq::dtag_value const& value) const noexcept {
    return value.hash();
  }
};

template<>
struct hash<frq::etag_value> {
  constexpr inline size_t
      operator()(frq::etag_value const& /*unused*/) const noexcept {
    return 0;
  }
};

template<>
struct equal_to<frq::etag_value> {
  constexpr inline bool
      operator()(frq::etag_value const& /*unused*/,
                 frq::etag_value const& /*unused*/) const noexcept {
    return true;
  }
};

} // namespace std
