#pragma once

#include <h3c/export.hpp>
#include <h3c/settings.hpp>
#include <h3c/varint.hpp>

#include <cstddef>
#include <cstdint>
#include <system_error>

// https://quicwg.org/base-drafts/draft-ietf-quic-http.html#rfc.section.4

namespace h3c {

class logger;

class frame {
public:
  class encoder;
  class decoder;

  enum class type : uint64_t {
    data = 0x0,
    headers = 0x1,
    priority = 0x2,
    cancel_push = 0x3,
    settings = 0x4,
    push_promise = 0x5,
    goaway = 0x7,
    max_push_id = 0xd,
    duplicate_push = 0xe
  };

  struct payload {
    struct data {
      uint64_t size;
    };

    struct headers {
      uint64_t size;
    };

    struct priority {
      enum class type : uint8_t {
        request = 0x0,
        push = 0x1,
        placeholder = 0x2,
        // Only valid for prioritized_element_type.
        current = 0x3,
        // Only valid for element_dependency_type.
        root = 0x3
      };

      priority::type prioritized_element_type;
      priority::type element_dependency_type;
      uint64_t prioritized_element_id;
      uint64_t element_dependency_id;
      uint8_t weight;
    };

    using settings = h3c::settings;

    struct cancel_push {
      uint64_t push_id;
    };

    struct push_promise {
      uint64_t push_id;
      uint64_t size;
    };

    struct goaway {
      uint64_t stream_id;
    };

    struct max_push_id {
      uint64_t push_id;
    };

    struct duplicate_push {
      uint64_t push_id;
    };
  };

  // We allow implicit conversions from a frame payload into a frame.

  // clang-format off
  H3C_EXPORT explicit frame() noexcept;
  H3C_EXPORT frame(frame::payload::data data) noexcept;                     // NOLINT
  H3C_EXPORT frame(frame::payload::headers headers) noexcept;               // NOLINT
  H3C_EXPORT frame(frame::payload::priority priority) noexcept;             // NOLINT
  H3C_EXPORT frame(frame::payload::settings settings) noexcept;             // NOLINT
  H3C_EXPORT frame(frame::payload::cancel_push cancel_push) noexcept;       // NOLINT
  H3C_EXPORT frame(frame::payload::push_promise push_promise) noexcept;     // NOLINT
  H3C_EXPORT frame(frame::payload::goaway goaway) noexcept;                 // NOLINT
  H3C_EXPORT frame(frame::payload::max_push_id max_push_id) noexcept;       // NOLINT
  H3C_EXPORT frame(frame::payload::duplicate_push duplicate_push) noexcept; // NOLINT
  // clang-format on

  H3C_EXPORT operator type() const noexcept; // NOLINT

  H3C_EXPORT frame::payload::data data() const noexcept;
  H3C_EXPORT frame::payload::headers headers() const noexcept;
  H3C_EXPORT frame::payload::priority priority() const noexcept;
  H3C_EXPORT frame::payload::settings settings() const noexcept;
  H3C_EXPORT frame::payload::cancel_push cancel_push() const noexcept;
  H3C_EXPORT frame::payload::push_promise push_promise() const noexcept;
  H3C_EXPORT frame::payload::goaway goaway() const noexcept;
  H3C_EXPORT frame::payload::max_push_id max_push_id() const noexcept;
  H3C_EXPORT frame::payload::duplicate_push duplicate_push() const noexcept;

private:
  type type_; // NOLINT

  // We don't store the frame length since it's easier to calculate it when
  // needed. This also prevents it from getting stale.

  union {
    payload::data data_;
    payload::headers headers_;
    payload::priority priority_;
    payload::settings settings_;
    payload::cancel_push cancel_push_;
    payload::push_promise push_promise_;
    payload::goaway goaway_;
    payload::max_push_id max_push_id_;
    payload::duplicate_push duplicate_push_;
  };
};

class frame::encoder {
public:
  H3C_EXPORT explicit encoder(const logger *logger) noexcept;

  encoder(const encoder &) = delete;
  encoder &operator=(const encoder &) = delete;

  encoder(encoder &&) = default;
  encoder &operator=(encoder &&) = default;

  ~encoder() = default;

  H3C_EXPORT size_t encoded_size(const frame &frame) const noexcept;

  H3C_EXPORT std::error_code encode(uint8_t *dest,
                                    size_t size,
                                    const frame &frame,
                                    size_t *encoded_size) const noexcept;

private:
  varint::encoder varint;
  const logger *logger;

  uint64_t payload_size(const frame &frame) const noexcept;
};

class frame::decoder {
public:
  H3C_EXPORT explicit decoder(const logger *logger) noexcept;

  decoder(const decoder &) = delete;
  decoder &operator=(const decoder &) = delete;

  decoder(decoder &&) = default;
  decoder &operator=(decoder &&) = default;

  ~decoder() = default;

  H3C_EXPORT std::error_code decode(const uint8_t *src,
                                    size_t size,
                                    frame *frame,
                                    size_t *encoded_size) const noexcept;

private:
  varint::decoder varint;
  const logger *logger;
};

} // namespace h3c
