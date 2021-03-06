#include <bnl/http3/codec/headers.hpp>

namespace bnl {
namespace http3 {
namespace headers {

result<void>
encoder::add(header_view header)
{
  if (state_ != state::idle) {
    return error::internal;
  }

  base::buffer encoded = BNL_TRY(qpack_.encode(header));
  buffers_.push(std::move(encoded));

  return base::success();
}

result<void>
encoder::fin() noexcept
{
  if (state_ != state::idle) {
    return error::internal;
  }

  state_ = state::frame;

  return base::success();
}

bool
encoder::finished() const noexcept
{
  return state_ == state::fin;
}

result<base::buffer>
encoder::encode() noexcept
{
  switch (state_) {

    case state::idle:
      return error::idle;

    case state::frame: {
      frame frame = frame::payload::headers{ qpack_.count() };

      base::buffer encoded = BNL_TRY(frame::encode(frame));

      state_ = state::qpack;

      return encoded;
    }

    case state::qpack: {
      base::buffer encoded = buffers_.pop();
      state_ = buffers_.empty() ? state::fin : state_;

      return encoded;
    }

    case state::fin:
      return error::internal;
  }

  return error::internal;
}

bool
decoder::started() const noexcept
{
  return state_ != state::frame;
}

bool
decoder::finished() const noexcept
{
  return state_ == state::fin;
}

template<typename Sequence>
result<header>
decoder::decode(Sequence &encoded)
{
  switch (state_) {

    case state::frame: {
      frame::type type = BNL_TRY(frame::peek(encoded));

      if (type != frame::type::headers) {
        return error::delegate;
      }

      frame frame = BNL_TRY(frame::decode(encoded));

      state_ = state::qpack;
      headers_size_ = frame.headers.size;

      if (headers_size_ == 0) {
        return error::malformed_frame;
      }
    }
    /* FALLTHRU */
    case state::qpack: {
      header header = BNL_TRY(qpack_.decode(encoded));

      if (qpack_.count() > headers_size_) {
        return error::malformed_frame;
      }

      bool fin = qpack_.count() == headers_size_;
      state_ = fin ? state::fin : state_;

      return header;
    }

    case state::fin:
      return error::internal;
  }

  return error::internal;
}

BNL_BASE_SEQUENCE_IMPL(BNL_HTTP3_HEADERS_DECODE_IMPL);

}
}
}
