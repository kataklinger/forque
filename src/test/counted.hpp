#pragma once

class counted {
private:
  static inline int instance_count_{0};

  static inline int default_count_{0};
  static inline int copy_count_{0};
  static inline int move_count_{0};

  static inline int destruct_count_{0};

public:
  inline static void reset() noexcept {
    instance_count_ = default_count_ = copy_count_ = move_count_ =
        destruct_count_ = 0;
  }

  static inline int get_instances() noexcept {
    return instance_count_;
  }

  static inline int get_defaults() noexcept {
    return default_count_;
  }

  static inline int get_copies() noexcept {
    return copy_count_;
  }

  static inline int get_moves() noexcept {
    return move_count_;
  }

  static inline int get_destructs() noexcept {
    return destruct_count_;
  }

public:
  inline counted() noexcept {
    ++instance_count_;
    ++default_count_;
  }

  inline counted(counted const& /*unused*/) noexcept {
    ++instance_count_;
    ++copy_count_;
  }

  inline counted(counted&& /*unused*/) noexcept {
    ++instance_count_;
    ++move_count_;
  }

  inline ~counted() {
    --instance_count_;
    ++destruct_count_;
  }

  counted& operator=(counted const&) = delete;
  counted& operator=(counted&&) = delete;
};

inline std::string to_string(counted const& /*unused*/) {
  return std::string{"{}"};
}

class counted_guard {
public:
  inline counted_guard() noexcept {
    counted::reset();
  }

  inline ~counted_guard() {
    counted::reset();
  }

  counted_guard(counted_guard const&) = delete;
  counted_guard(counted_guard&&) = delete;

  counted_guard& operator=(counted_guard const&) = delete;
  counted_guard& operator=(counted_guard&&) = delete;

  inline auto instance() {
    return counted{};
  }
};
