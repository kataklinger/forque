#pragma once

#include "tag.hpp"

#include <ostream>
#include <tuple>
#include <vector>

namespace frq {

namespace detail {
  template<typename Tuple, std::size_t... Idxs>
  void tag_stream_helper(std::ostream& stream,
                         Tuple const& values,
                         std::index_sequence<Idxs...>) {
    //(stream << ... << std::get<Idxs>(values));
    auto formatter = [&stream](auto const& value) { stream << '/' << value; };
    (formatter(std::get<Idxs>(values)), ...);
  }

  template<typename... Tys>
  void tag_stream_helper(std::ostream& stream,
                         std::tuple<Tys...> const& values) {
    tag_stream_helper(stream, values, std::index_sequence_for<Tys...>{});
  }

  template<typename Alloc>
  void tag_stream_helper(std::ostream& stream,
                         std::vector<frq::dtag_value, Alloc> const& values) {
    for (auto& value : values) {
      stream << '/' << value.get_string();
    }
  }

} // namespace detail

template<taglike Tag>
auto& operator<<(std::ostream& stream, Tag const& tag) {
  detail::tag_stream_helper(stream, tag.values());
  return stream;
}

} // namespace frq
