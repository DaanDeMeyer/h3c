#pragma once

#include <bnl/base/buffers.hpp>
#include <bnl/quic/error.hpp>
#include <bnl/quic/export.hpp>
#include <bnl/result.hpp>

namespace bnl {

namespace log {
class api;
}

namespace quic {
namespace client {

namespace ngtcp2 {
class connection;
}

class BNL_QUIC_EXPORT stream {
public:
  stream(uint64_t id, ngtcp2::connection *ngtcp2, const log::api *logger);

  result<base::buffer> send();

  result<void> add(base::buffer buffer);
  result<void> fin();

  result<void> ack(size_t size);

  bool finished() const noexcept;
  bool opened() const noexcept;

private:
  base::buffers buffers_;
  base::buffers keepalive_;
  bool fin_ = false;
  bool opened_ = false;

  uint64_t id_;
  ngtcp2::connection *ngtcp2_;
  const log::api *logger_;
};

}
}
}
