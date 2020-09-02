
#include "../inc/key.hpp"

#include "counted.hpp"

#include "gtest/gtest.h"

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

namespace std {
template<>
struct hash<counted> {
  inline std::size_t operator()(counted const& /*unused*/) const noexcept {
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

template<typename... Tys>
using default_static_key = frq::static_key<frq::default_hash_compare, Tys...>;

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

class static_key_default_hash_cmp_tests : public testing::Test {
protected:
  using key_type = default_static_key<int, float>;

  void SetUp() override {
  }

  key_type key1_{frq::construct_key_default, 1, 1.0F};
  key_type key2_{frq::construct_key_default, 2, 2.0F};

  using index0_type = std::integral_constant<key_type::size_type, 0>;
  using index1_type = std::integral_constant<key_type::size_type, 1>;
};

TEST_F(static_key_default_hash_cmp_tests, hash) {
  EXPECT_EQ(std::hash<int>{}(1), key1_.hash(index0_type{}));
  EXPECT_EQ(std::hash<float>{}(1.0f), key1_.hash(index1_type{}));
}

TEST_F(static_key_default_hash_cmp_tests, equal) {
  EXPECT_TRUE(key1_.equal(key1_, index0_type{}));
  EXPECT_TRUE(key1_.equal(key1_, index1_type{}));
}

TEST_F(static_key_default_hash_cmp_tests, not_equal) {
  EXPECT_FALSE(key1_.equal(key2_, index0_type{}));
  EXPECT_FALSE(key1_.equal(key2_, index1_type{}));
}

class static_key_custom_hash_cmp_tests : public testing::Test {
protected:
  using key_type = frq::static_key<custom_hash_compare, int, float>;

  void SetUp() override {
  }

  key_type key1_{frq::construct_key_hash_cmp,
                 custom_hash_compare{true},
                 1,
                 1.0F};
  key_type key2_{frq::construct_key_hash_cmp,
                 custom_hash_compare{true},
                 2,
                 2.0F};

  using index0_type = std::integral_constant<key_type::size_type, 0>;
  using index1_type = std::integral_constant<key_type::size_type, 1>;
};

TEST_F(static_key_custom_hash_cmp_tests, hash) {
  EXPECT_EQ(std::hash<int>{}(1), key1_.hash(index0_type{}));
  EXPECT_EQ(std::hash<float>{}(1.0f), key1_.hash(index1_type{}));
}

TEST_F(static_key_custom_hash_cmp_tests, equal) {
  EXPECT_TRUE(key1_.equal(key1_, index0_type{}));
  EXPECT_TRUE(key1_.equal(key1_, index1_type{}));
}

TEST_F(static_key_custom_hash_cmp_tests, not_equal) {
  EXPECT_FALSE(key1_.equal(key2_, index0_type{}));
  EXPECT_FALSE(key1_.equal(key2_, index1_type{}));
}

class static_key_view_tests : public testing::Test {
protected:
  using key_type = default_static_key<counted, int, float>;

  std::shared_ptr<key_type> make_key(int data) {
    return std::make_shared<key_type>(frq::construct_key_default,
                                      counter_.instance(),
                                      data,
                                      static_cast<float>(data));
  }

  void SetUp() override {
    key_ = make_key(1);
  }

  counted_guard counter_;
  std::shared_ptr<key_type> key_;
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

TEST_F(static_key_view_tests, key_ownership) {
  {
    frq::static_key_view_t<key_type, 0> view{key_};
    key_ = nullptr;

    EXPECT_EQ(1, counted::get_instances());
  }

  EXPECT_EQ(0, counted::get_instances());
}

TEST_F(static_key_view_tests, is_empty) {
  frq::static_key_view_t<key_type, 2> view{nullptr};

  EXPECT_TRUE(view.is_empty());
}

TEST_F(static_key_view_tests, hash) {
  frq::static_key_view_t<key_type, 1> view{key_};

  EXPECT_EQ(std::hash<int>{}(1), view.hash());
}

TEST_F(static_key_view_tests, swap) {
  frq::static_key_view_t<key_type, 2> view1{key_};
  frq::static_key_view_t<key_type, 2> view2{nullptr};

  view1.swap(view2);

  EXPECT_TRUE(view1.is_empty());
  EXPECT_FALSE(view2.is_empty());
}

TEST_F(static_key_view_tests, key_view_hash) {
  frq::static_key_view_t<key_type, 2> view{key_};
  EXPECT_EQ(std::hash<float>{}(1.0F), frq::key_view_hash{}(view));
}

TEST_F(static_key_view_tests, key_view_equal_to) {
  frq::static_key_view_t<key_type, 2> view{key_};
  EXPECT_TRUE(frq::key_view_equal_to{}(view, view));
}

TEST_F(static_key_view_tests, equal) {
  frq::static_key_view_t<key_type, 2> view{key_};

  EXPECT_TRUE(view.equal(view));
}

class static_key_view_multiple_tests : public static_key_view_tests {
protected:
  void SetUp() override {
    static_key_view_tests::SetUp();
    key2_ = make_key(2);
  }

  std::shared_ptr<key_type> key2_;
};

TEST_F(static_key_view_multiple_tests, not_equal) {
  frq::static_key_view_t<key_type, 2> view1{key_};
  frq::static_key_view_t<key_type, 2> view2{key2_};

  EXPECT_FALSE(view1.equal(view2));
}

TEST(make_dynamic_key_node_tests, construct) {
  counted_guard guard{};
  {
    auto node = frq::make_dynamic_key_node<counted>(
        std::allocator<counted>{}, custom_hash_compare{}, guard.instance());

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
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

  EXPECT_EQ(std::hash<int>{}(1), key.hash(1));
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

  std::vector<
      frq::dynamic_key_node_pointer<std::allocator<frq::dynamic_key_node>>>
      nodes{};

  nodes.push_back(frq::make_dynamic_key_node<counted>(
      std::allocator<frq::dynamic_key_node>{},
      custom_hash_compare{},
      guard.instance()));

  nodes.push_back(frq::make_dynamic_key_node<int>(
      std::allocator<frq::dynamic_key_node>{}, custom_hash_compare{}, 1));

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

TEST_F(dynamic_key_tests, hash) {
  EXPECT_EQ(std::hash<float>{}(1.0F), key_.hash(1U));
}

TEST_F(dynamic_key_tests, equal) {
  EXPECT_TRUE(key_.equal(key_, 0U));
}

TEST_F(dynamic_key_tests, equal_same_type) {
  frq::dynamic_key<> other{frq::construct_key_default, 2, 2.0F};

  EXPECT_FALSE(key_.equal(other, 1U));
}

TEST_F(dynamic_key_tests, equal_different_type) {
  frq::dynamic_key<> other{frq::construct_key_default, 2, 2};

  EXPECT_FALSE(key_.equal(other, 1U));
}

class dynamic_key_view_tests : public testing::Test {
protected:
  using key_type = frq::dynamic_key<>;

  void SetUp() override {
    key_ = std::make_shared<key_type>(frq::construct_key_default, 1, 1.0F);
  }

  std::shared_ptr<key_type> key_;
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

TEST_F(dynamic_key_view_tests, hash) {
  frq::dynamic_key_view_t<key_type> view{key_, 0U};

  EXPECT_EQ(std::hash<int>{}(1), view.hash());
}

TEST_F(dynamic_key_view_tests, equal) {
  frq::dynamic_key_view_t<key_type> view{key_, 0U};

  EXPECT_TRUE(view.equal(view));
}

TEST_F(dynamic_key_view_tests, not_equal) {
  frq::dynamic_key_view_t<key_type> view1{key_, 0U};
  frq::dynamic_key_view_t<key_type> view2{key_, 1U};

  EXPECT_FALSE(view1.equal(view2));
}

TEST_F(dynamic_key_view_tests, is_empty) {
  frq::dynamic_key_view_t<key_type> view{};

  EXPECT_TRUE(view.is_empty());
}

TEST_F(dynamic_key_view_tests, swap) {
  frq::dynamic_key_view_t<key_type> view1{key_, 0U};
  frq::dynamic_key_view_t<key_type> view2{};

  view1.swap(view2);

  EXPECT_TRUE(view1.is_empty());
  EXPECT_EQ(0U, view1.depth());

  EXPECT_FALSE(view2.is_empty());
  EXPECT_EQ(2U, view2.depth());
}
