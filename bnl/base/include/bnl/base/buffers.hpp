#pragma once

#include <bnl/base/buffer.hpp>
#include <bnl/base/export.hpp>

#include <list>

namespace bnl {
namespace base {

class BNL_BASE_EXPORT buffers {
public:
  class lookahead;

  using lookahead_type = lookahead;

  buffers() = default;

  buffers(buffers &&) = default;
  buffers &operator=(buffers &&) = default;

  size_t size() const noexcept;
  bool empty() const noexcept;

  uint8_t operator[](size_t index) const noexcept;
  uint8_t operator*() const noexcept;

  buffer slice(size_t size);

  void push(buffer buffer);
  buffer pop();

  const buffer &front() const noexcept;
  const buffer &back() const noexcept;

  void consume(size_t size) noexcept;

private:
  void discard();

  buffer concat(std::list<buffer>::iterator start,
                std::list<buffer>::iterator end,
                size_t left) const;

private:
  std::list<buffer> buffers_;
};

class BNL_BASE_EXPORT buffers::lookahead {
public:
  using lookahead_type = lookahead;

  lookahead(const buffers &buffers) noexcept; // NOLINT

  lookahead(const lookahead &other) noexcept;
  lookahead &operator=(const lookahead &) = delete;

  lookahead(lookahead &&) = delete;
  lookahead &operator=(lookahead &&) = delete;

  ~lookahead() = default;

  size_t size() const noexcept;
  bool empty() const noexcept;

  uint8_t operator[](size_t index) const noexcept;
  uint8_t operator*() const noexcept;

  void consume(size_t size) noexcept;
  size_t consumed() const noexcept;

private:
  const buffers &buffers_;
  size_t previous_ = 0;
  size_t position_ = 0;
};

}
}
