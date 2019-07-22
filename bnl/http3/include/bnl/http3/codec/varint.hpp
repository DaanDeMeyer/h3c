#pragma once

#include <bnl/base/buffer.hpp>
#include <bnl/base/buffers.hpp>
#include <bnl/base/template.hpp>
#include <bnl/http3/export.hpp>
#include <bnl/result.hpp>

#include <cstddef>
#include <cstdint>

// https://quicwg.org/base-drafts/draft-ietf-quic-transport.html#integer-encoding

namespace bnl {

namespace log {
class api;
}

namespace http3 {
namespace varint {

static constexpr uint64_t max = (0x40ULL << 56U) - 1;

class BNL_HTTP3_EXPORT encoder {
public:
  explicit encoder(const log::api *logger) noexcept;

  encoder(encoder &&) = default;
  encoder &operator=(encoder &&) = default;

  result<size_t> encoded_size(uint64_t varint) const noexcept;

  result<size_t> encode(uint8_t *dest, uint64_t varint) const noexcept;

  result<base::buffer> encode(uint64_t varint) const;

private:
  const log::api *logger_;
};

class BNL_HTTP3_EXPORT decoder {
public:
  explicit decoder(const log::api *logger) noexcept;

  decoder(decoder &&) = default;
  decoder &operator=(decoder &&) = default;

  template<typename Sequence>
  result<uint64_t> decode(Sequence &encoded) const noexcept;

private:
  const log::api *logger_;
};

#define BNL_HTTP3_VARINT_DECODE_IMPL(T)                                        \
  template BNL_HTTP3_EXPORT result<uint64_t> decoder::decode<T>(               \
    T &) /* NOLINT */ const noexcept

BNL_BASE_SEQUENCE_DECL(BNL_HTTP3_VARINT_DECODE_IMPL);
BNL_BASE_LOOKAHEAD_DECL(BNL_HTTP3_VARINT_DECODE_IMPL);

}

}
}
