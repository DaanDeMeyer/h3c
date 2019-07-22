#include <bnl/http3/codec/body.hpp>

#include <bnl/base/error.hpp>
#include <bnl/http3/error.hpp>
#include <bnl/util/error.hpp>

namespace bnl {
namespace http3 {
namespace body {

encoder::encoder(const log::api *logger) noexcept
  : frame_(logger)
  , logger_(logger)
{}

result<void>
encoder::add(base::buffer body)
{
  if (fin_) {
    THROW(connection::error::internal);
  }

  buffers_.emplace(std::move(body));

  return bnl::success();
}

result<void>
encoder::fin() noexcept
{
  if (state_ == state::fin) {
    THROW(connection::error::internal);
  }

  fin_ = true;

  if (buffers_.empty()) {
    state_ = state::fin;
  }

  return bnl::success();
}

bool
encoder::finished() const noexcept
{
  return state_ == state::fin;
}

result<base::buffer>
encoder::encode() noexcept
{
  // TODO: Implement PRIORITY

  switch (state_) {

    case state::frame: {
      if (buffers_.empty()) {
        return base::error::idle;
      }

      frame frame = frame::payload::data{ buffers_.front().size() };
      base::buffer encoded = TRY(frame_.encode(frame));

      state_ = state::data;

      return encoded;
    }

    case state::data: {
      base::buffer body = std::move(buffers_.front());
      buffers_.pop();

      state_ = fin_ && buffers_.empty() ? state::fin : state::frame;

      return body;
    }

    case state::fin:
      THROW(connection::error::internal);
  }

  NOTREACHED();
}

decoder::decoder(const log::api *logger) noexcept
  : frame_(logger)
  , logger_(logger)
{}

bool
decoder::in_progress() const noexcept
{
  return state_ == decoder::state::data;
}

template<typename Sequence>
result<base::buffer>
decoder::decode(Sequence &encoded)
{
  switch (state_) {

    case state::frame: {
      frame::type type = TRY(frame_.peek(encoded));

      if (type != frame::type::data) {
        return base::error::delegate;
      }

      frame frame = TRY(frame_.decode(encoded));

      state_ = state::data;
      remaining_ = frame.data.size;
    }
    /* FALLTHRU */
    case state::data: {
      if (encoded.empty()) {
        return base::error::incomplete;
      }

      size_t body_part_size = encoded.size() < remaining_
                                ? encoded.size()
                                : static_cast<size_t>(remaining_);
      base::buffer body_part = encoded.slice(body_part_size);

      remaining_ -= body_part_size;

      assert(encoded.empty() || remaining_ == 0);

      state_ = remaining_ == 0 ? state::frame : state_;

      return body_part;
    }
  }

  NOTREACHED();
}

BNL_BASE_SEQUENCE_IMPL(BNL_HTTP3_BODY_DECODE_IMPL);

}
}
}
