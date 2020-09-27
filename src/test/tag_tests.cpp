
#include "../inc/tag.hpp"

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

template<typename View, typename Key, typename Sub, typename Next>
struct check_tag_view_types {
  static constexpr bool valid = std::is_same_v<typename View::key_type, Key> &&
                                std::is_same_v<typename View::sub_type, Sub> &&
                                std::is_same_v<typename View::next_type, Next>;
};

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

static_assert(frq::stag_t<counted, int>::size == 2);

TEST(stag_constructor_tests, direct_move_construct) {
  counted_guard guard{};
  {
    frq::stag_t<counted, int> tag{
        frq::construct_tag_default, guard.instance(), 1};

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(stag_constructor_tests, direct_copy_construct) {
  counted_guard guard{};
  {
    auto instance = guard.instance();
    frq::stag_t<counted, int> tag{frq::construct_tag_default, instance, 1};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(0, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(stag_constructor_tests, tuple_move_construct) {
  counted_guard guard{};
  {
    frq::stag_t<counted, int> tag{
        std::tuple<counted, int>{guard.instance(), 1}};

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(2, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(stag_constructor_tests, tuple_copy_construct) {
  counted_guard guard{};
  {
    std::tuple<counted, int> pack{guard.instance(), 1};
    frq::stag_t<counted, int> tag{pack};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(stag_constructor_tests, move_construct) {
  counted_guard guard{};
  {
    frq::stag_t<counted, int> tag1{
        frq::construct_tag_default, guard.instance(), 1};
    frq::stag_t<counted, int> tag2{std::move(tag1)};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(2, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(stag_constructor_tests, copy_construct) {
  counted_guard guard{};
  {
    frq::stag_t<counted, int> tag1{
        frq::construct_tag_default, guard.instance(), 1};
    frq::stag_t<counted, int> tag2{tag1};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(stag_value_tests, get_value) {
  frq::stag_t<float, int> tag{frq::construct_tag_default, 1.0F, 2};

  EXPECT_EQ(1.0F, get<0>(tag.values()));
  EXPECT_EQ(2, get<1>(tag.values()));
}

using test_stag = frq::stag_t<float, int>;

template<test_stag::size_type Level>
using test_sview = frq::stag_view<test_stag, Level>;

static_assert(check_tag_view_types<test_sview<0>,
                                   float,
                                   frq::stag<1, float, int>,
                                   test_sview<1>>::valid);

static_assert(check_tag_view_types<test_sview<1>,
                                   int,
                                   frq::stag<2, float, int>,
                                   test_sview<1>>::valid);

class stag_view_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  test_stag tag_{frq::construct_tag_default, 1.0F, 2};
};

TEST_F(stag_view_tests, first_view_key) {
  auto view = frq::view(tag_);

  EXPECT_EQ(1.0F, view.key());
}

TEST_F(stag_view_tests, first_view_sub) {
  auto view = frq::view(tag_);

  auto value = get<0>(view.sub().values());
  EXPECT_EQ(1.0F, value);
}

TEST_F(stag_view_tests, first_view_next) {
  auto view = frq::view(tag_);

  EXPECT_EQ(2, view.next().key());
}

TEST_F(stag_view_tests, first_view_last) {
  auto view = frq::view(tag_);

  EXPECT_FALSE(view.last());
}

TEST_F(stag_view_tests, last_view_key) {
  test_sview<1> view{tag_};

  EXPECT_EQ(2, view.key());
}

TEST_F(stag_view_tests, last_view_sub) {
  test_sview<1> view{tag_};

  auto value = get<1>(view.sub().values());
  EXPECT_EQ(2, value);
}

TEST_F(stag_view_tests, last_view_next) {
  test_sview<1> view{tag_};

  EXPECT_EQ(2, view.next().key());
}

TEST_F(stag_view_tests, last_view_last) {
  test_sview<1> view{tag_};

  EXPECT_TRUE(view.last());
}

TEST(dtag_node_make_tests, construct) {
  counted_guard guard{};
  {
    auto node = frq::make_dtag_node<counted>(
        std::allocator<counted>{}, custom_hash_compare{true}, guard.instance());

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

class dtag_value_tests : public testing::Test {
protected:
  void SetUp() override {
    value_ = frq::make_dtag_node<int>(
        std::allocator<int>{}, custom_hash_compare{true}, 1);
  }

  frq::dtag_value value_{nullptr};
};

TEST_F(dtag_value_tests, hash) {
  auto expected = std::hash<int>{}(1);
  EXPECT_EQ(expected, value_.hash());
}

TEST_F(dtag_value_tests, equality_equal) {
  EXPECT_TRUE(value_ == value_);
  EXPECT_FALSE(value_ != value_);
}

TEST_F(dtag_value_tests, equality_not_equal) {
  auto other = frq::make_dtag_node<int>(
      std::allocator<int>{}, custom_hash_compare{true}, 2);

  EXPECT_FALSE(value_ == other);
  EXPECT_TRUE(value_ != other);
}

TEST(dtag_constructor_tests, direct_move_construct) {
  counted_guard guard{};
  {
    frq::dtag<> tag{frq::construct_tag_default, guard.instance(), 1};

    EXPECT_EQ(1, counted::get_instances());
    EXPECT_EQ(0, counted::get_copies());
    EXPECT_EQ(1, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(dtag_constructor_tests, direct_copy_construct) {
  counted_guard guard{};
  {
    auto instance = guard.instance();
    frq::dtag<> tag{frq::construct_tag_default, instance, 1};

    EXPECT_EQ(2, counted::get_instances());
    EXPECT_EQ(1, counted::get_copies());
    EXPECT_EQ(0, counted::get_moves());
  }
  EXPECT_EQ(0, counted::get_instances());
}

TEST(dtag_constructor_tests, direct_hash_construct) {
  counted_guard guard{};
  frq::dtag<> tag{frq::construct_tag_hash_cmp,
                  custom_hash_compare{true},
                  guard.instance(),
                  1};

  auto expected = std::hash<int>{}(1);
  EXPECT_EQ(expected, tag.values()[1].hash());
}

TEST(dtag_constructor_tests, direct_alloc_construct) {
  counted_guard guard{};
  frq::dtag<> tag{frq::construct_tag_alloc,
                  frq::dtag<>::allocator_type{},
                  guard.instance(),
                  1};

  EXPECT_EQ(1, counted::get_instances());
}

TEST(dtag_constructor_tests, range_construct) {
  counted_guard guard{};

  std::vector<frq::dtag_value> nodes{};

  nodes.push_back(frq::make_dtag_node<counted>(std::allocator<frq::dtag_node>{},
                                               custom_hash_compare{true},
                                               guard.instance()));

  nodes.push_back(frq::make_dtag_node<int>(
      std::allocator<frq::dtag_node>{}, custom_hash_compare{true}, 1));

  frq::dtag<> tag{std::begin(nodes), std::end(nodes)};

  EXPECT_EQ(1, counted::get_instances());
  EXPECT_EQ(2U, tag.values().size());
}

class dtag_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::dtag<> tag_{frq::construct_tag_default, 1.0F, 2};
};

TEST_F(dtag_tests, get_values) {
  EXPECT_EQ(2U, tag_.values().size());
}

TEST_F(dtag_tests, get_pack_matched_types) {
  auto [value1, value2] = tag_.pack<float, int>();

  EXPECT_EQ(1.0F, value1);
  EXPECT_EQ(2, value2);
}

TEST_F(dtag_tests, get_values_mimatched_types) {
  auto wrapper = [this] { tag_.pack<double, int>(); };
  EXPECT_THROW(wrapper(), std::bad_cast);
}

using test_dview = frq::dtag_view<frq::dtag<>>;

static_assert(
    check_tag_view_types<test_dview, frq::dtag_value, frq::dtag<>, test_dview>::
        valid);

class dtag_view_tests : public testing::Test {
protected:
  void SetUp() override {
  }

  frq::dtag<> tag_{frq::construct_tag_default, 1.0F, 2};
};

TEST_F(dtag_view_tests, first_view_key) {
  auto view = frq::view(tag_);

  auto expected = std::hash<float>{}(1.0F);
  EXPECT_EQ(expected, view.key().hash());
}

TEST_F(dtag_view_tests, first_view_sub) {
  auto view = frq::view(tag_);

  auto [value] = view.sub().pack<float>();
  EXPECT_EQ(1.0F, value);
}

TEST_F(dtag_view_tests, first_view_next) {
  auto view = frq::view(tag_);

  auto expected = std::hash<int>{}(2);
  EXPECT_EQ(expected, view.next().key().hash());
}

TEST_F(dtag_view_tests, first_view_last) {
  auto view = frq::view(tag_);

  EXPECT_FALSE(view.last());
}

TEST_F(dtag_view_tests, last_view_key) {
  test_dview view{tag_, 1};

  auto expected = std::hash<int>{}(2);
  EXPECT_EQ(expected, view.key().hash());
}

TEST_F(dtag_view_tests, last_view_sub) {
  test_dview view{tag_, 1};

  auto [value1, value2] = view.sub().pack<float, int>();
  EXPECT_EQ(2, value2);
}

TEST_F(dtag_view_tests, last_view_next) {
  test_dview view{tag_, 1};

  auto expected = std::hash<int>{}(2);
  EXPECT_EQ(expected, view.next().key().hash());
}

TEST_F(dtag_view_tests, last_view_last) {
  test_dview view{tag_, 1};

  EXPECT_TRUE(view.last());
}

static_assert(frq::tag_view_traits<test_sview<0>>::is_static);
static_assert(!frq::tag_view_traits<test_sview<0>>::is_last);
static_assert(frq::tag_view_traits<test_sview<1>>::is_last);

static_assert(!frq::tag_view_traits<test_dview>::is_static);
static_assert(!frq::tag_view_traits<test_dview>::is_last);
