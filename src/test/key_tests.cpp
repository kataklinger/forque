
#include "../inc/key.hpp"

#include "counted.hpp"

#include "gtest/gtest.h"

namespace std {
template<>
struct hash<counted> {
  inline size_t operator()(counted const& /*unused*/) const noexcept {
    return 0;
  }
};

template<>
struct equal_to<counted> {
  inline bool operator()(counted const& /*unused*/,
                         counted const& /*unused*/) const noexcept {
    return true;
  }
};
} // namespace std

struct custom_hash_compare {
  template<typename Ty>
  inline std::size_t hash(Ty const& value) const noexcept {
    assert(initialized);
    return std::hash<Ty>{}(value);
  }

  template<typename Ty>
  inline bool equal_to(Ty const& left, Ty const& right) const noexcept {
    assert(initialized);
    return std::equal_to<Ty>{}(left, right);
  }

  bool initialized{false};
};

template<typename... Tys>
using default_static_key = frq::static_key<Tys...>;

static_assert(default_static_key<counted, int>::depth == 2);

TEST(static_key_constructor_tests, direct_move_construct) {
  counted_guard guard{};
  {
    default_static_key<counted, int> key{
        frq::construct_key_default, guard.instance(), 1};

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(static_key_constructor_tests, direct_copy_construct) {
  counted_guard guard{};
  {
    auto instance = guard.instance();
    default_static_key<counted, int> key{
        frq::construct_key_default, instance, 1};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(0, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(static_key_constructor_tests, tuple_move_construct) {
  counted_guard guard{};
  {
    default_static_key<counted, int> key{
        std::tuple<counted, int>{guard.instance(), 1}};

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(2, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(static_key_constructor_tests, tuple_copy_construct) {
  counted_guard guard{};
  {
    std::tuple<counted, int> pack{guard.instance(), 1};
    default_static_key key{pack};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(static_key_constructor_tests, move_construct) {
  counted_guard guard{};
  {
    default_static_key<counted, int> key1{
        frq::construct_key_default, guard.instance(), 1};
    default_static_key<counted, int> key2{std::move(key1)};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(2, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(static_key_constructor_tests, copy_construct) {
  counted_guard guard{};
  {
    default_static_key<counted, int> key1{
        frq::construct_key_default, guard.instance(), 1};
    default_static_key<counted, int> key2{key1};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

class static_key_tests : public testing::Test {
protected:
  using key_type = default_static_key<int, float>;

  void SetUp() override {
  }

  key_type key1_{frq::construct_key_default, 1, 1.0F};
  key_type key2_{frq::construct_key_default, 2, 2.0F};

  using index0_type = std::integral_constant<key_type::size_type, 0>;
  using index1_type = std::integral_constant<key_type::size_type, 1>;
};

TEST_F(static_key_tests, value) {
  EXPECT_EQ(1, key1_.value(index0_type{}));
  EXPECT_EQ(1.0F, key1_.value(index1_type{}));
}

class static_key_view_tests : public testing::Test {
protected:
  using key_type = default_static_key<counted, int, float>;

  void SetUp() override {
  }

  counted_guard counter_;
  key_type key_{frq::construct_key_default, counter_.instance(), 1, 1.0F};
};

TEST_F(static_key_view_tests, constants) {
  frq::static_key_view_t<key_type, 1> view{key_};

  EXPECT_EQ(1, view.current());
  EXPECT_EQ(3, view.depth());
  EXPECT_FALSE(view.is_last());
}

TEST_F(static_key_view_tests, constants_last) {
  frq::static_key_view_t<key_type, 2> view{key_};

  EXPECT_EQ(2, view.current());
  EXPECT_TRUE(view.is_last());
}

TEST_F(static_key_view_tests, constants_truncated) {
  frq::static_key_view_t<key_type, 1, 2> view{key_};

  EXPECT_EQ(2, view.depth());
  EXPECT_TRUE(view.is_last());
}

TEST_F(static_key_view_tests, next_from_first) {
  frq::static_key_view_t<key_type, 0> view_first{key_};
  auto view_next = view_first.next();

  static_assert(
      std::is_same_v<decltype(view_next), frq::static_key_view_t<key_type, 1>>);

  EXPECT_EQ(1, view_next.current());
}

TEST_F(static_key_view_tests, next_from_last) {
  frq::static_key_view_t<key_type, 2> view_first{key_};
  auto view_next = view_first.next();

  static_assert(
      std::is_same_v<decltype(view_next), frq::static_key_view_t<key_type, 2>>);

  EXPECT_EQ(2, view_next.current());
}

TEST_F(static_key_view_tests, value) {
  frq::static_key_view_t<key_type, 2> view{key_};

  EXPECT_EQ(1.0F, view.value());
}

TEST(dynamic_key_node_make_tests, construct) {
  counted_guard guard{};
  {
    auto node = frq::make_dynamic_key_node<counted>(
        std::allocator<counted>{}, custom_hash_compare{true}, guard.instance());

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

class dynamic_key_node_tests : public testing::Test {
protected:
  void SetUp() override {
    node_ = frq::make_dynamic_key_node<int>(
        std::allocator<int>{}, custom_hash_compare{true}, 1);
  }

  frq::dynamic_key_node_pointer node_;
};

TEST_F(dynamic_key_node_tests, hash) {
  EXPECT_EQ(std::hash<int>{}(1), node_->hash());
}

TEST_F(dynamic_key_node_tests, equal) {
  EXPECT_TRUE(node_->equal(*node_));
}

TEST_F(dynamic_key_node_tests, not_equal) {
  auto other = frq::make_dynamic_key_node<int>(
      std::allocator<int>{}, custom_hash_compare{true}, 2);

  EXPECT_FALSE(node_->equal(*other));
}

TEST(dynamic_key_constructor_tests, direct_move_construct) {
  counted_guard guard{};
  {
    frq::dynamic_key<> key{frq::construct_key_default, guard.instance(), 1};

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(dynamic_key_constructor_tests, direct_copy_construct) {
  counted_guard guard{};
  {
    auto instance = guard.instance();
    frq::dynamic_key<> key{frq::construct_key_default, instance, 1};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(0, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(dynamic_key_constructor_tests, direct_hash_construct) {
  counted_guard guard{};
  frq::dynamic_key<> key{frq::construct_key_hash_cmp,
                         custom_hash_compare{true},
                         guard.instance(),
                         1};

  EXPECT_EQ(std::hash<int>{}(1), key.node(1)->hash());
}

TEST(dynamic_key_constructor_tests, direct_alloc_construct) {
  counted_guard guard{};
  frq::dynamic_key<> key{frq::construct_key_alloc,
                         frq::dynamic_key<>::allocator_type{},
                         guard.instance(),
                         1};

  EXPECT_EQ(1, counted::get_instances());
}

TEST(dynamic_key_constructor_tests, range_construct) {
  counted_guard guard{};

  std::vector<frq::dynamic_key_node_pointer> nodes{};

  nodes.push_back(frq::make_dynamic_key_node<counted>(
      std::allocator<frq::dynamic_key_node>{},
      custom_hash_compare{true},
      guard.instance()));

  nodes.push_back(frq::make_dynamic_key_node<int>(
      std::allocator<frq::dynamic_key_node>{}, custom_hash_compare{true}, 1));

  frq::dynamic_key<> key{std::begin(nodes), std::end(nodes)};

  EXPECT_EQ(1, counted::get_instances());
  EXPECT_EQ(2U, key.depth());
}

class dynamic_key_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::dynamic_key<> key_{frq::construct_key_default, 1, 1.0F};
};

TEST_F(dynamic_key_tests, depth) {
  EXPECT_EQ(2U, key_.depth());
}

TEST_F(dynamic_key_tests, get_values_matched_types) {
  auto values = key_.values<int, float>();

  EXPECT_EQ(1, get<0>(values));
  EXPECT_EQ(1.0F, get<1>(values));
}

TEST_F(dynamic_key_tests, get_values_mimatched_types) {
  auto wrapper = [this] { key_.values<int, double>(); };
  EXPECT_THROW(wrapper(), std::bad_cast);
}

TEST_F(dynamic_key_tests, node) {
  EXPECT_EQ(key_.node(1), key_.node(1));
}

class dynamic_key_view_tests : public testing::Test {
protected:
  using key_type = frq::dynamic_key<>;

  void SetUp() override {
  }

  key_type key_{frq::construct_key_default, 1, 1.0F};
};

TEST_F(dynamic_key_view_tests, state_first) {
  frq::dynamic_key_view_t<key_type> view{key_, 0U};

  EXPECT_EQ(0U, view.current());
  EXPECT_FALSE(view.is_last());
}

TEST_F(dynamic_key_view_tests, state_last) {
  frq::dynamic_key_view_t<key_type> view{key_, 1U};

  EXPECT_EQ(1U, view.current());
  EXPECT_TRUE(view.is_last());
}

TEST_F(dynamic_key_view_tests, depth) {
  frq::dynamic_key_view_t<key_type> view{key_, 1U};

  EXPECT_EQ(2U, view.depth());
}

TEST_F(dynamic_key_view_tests, next) {
  frq::dynamic_key_view_t<key_type> view{key_, 0U};
  auto next = view.next();

  EXPECT_EQ(1U, next.current());
}

TEST_F(dynamic_key_view_tests, value) {
  frq::dynamic_key_view_t<key_type> view{key_, 0U};

  EXPECT_EQ(view.value(), view.value());
}

TEST(dynamic_key_value_ownership_tests, keep_ownership) {
  using key_type = frq::dynamic_key<>;
  counted_guard guard{};

  std::unique_ptr<key_type> key =
      std::make_unique<key_type>(frq::construct_key_default, guard.instance());

  {
    auto value{frq::dynamic_key_view_t<key_type>{*key, 0U}.value()};
    key.reset();

    EXPECT_EQ(1, counted::get_instances());
  }

  EXPECT_EQ(0, counted::get_instances());
}

class dynamic_key_value_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::dynamic_key<> key_{frq::construct_key_default, 1, 1.0F};

  frq::dynamic_key_view_t<frq::dynamic_key<>> view1_{key_, 0U};
  frq::dynamic_key_view_t<frq::dynamic_key<>> view2_{key_, 1U};
};

TEST_F(dynamic_key_value_tests, hash) {
  auto value = view1_.value();
  EXPECT_EQ(std::hash<int>{}(1), std::hash<decltype(value)>{}(value));
}

TEST_F(dynamic_key_value_tests, equal) {
  EXPECT_TRUE(view1_.value() == view1_.value());
}

TEST_F(dynamic_key_value_tests, not_equal) {
  EXPECT_FALSE(view1_.value() == view2_.value());
}
