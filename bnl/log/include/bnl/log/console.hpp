#pragma once

#include <bnl/log.hpp>
#include <bnl/log/export.hpp>

namespace bnl {
namespace log {
namespace impl {

class BNL_LOG_EXPORT console : public log::api {
public:
  void log(log::level level,
           const char *file,
           const char *function,
           int line,
           const char *format,
           const fmt::format_args &args) const final;

private:
  log::level level_ = log::level::trace;
};

} // namespace impl
} // namespace log
} // namespace bnl