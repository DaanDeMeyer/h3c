#include <bnl/http3/client/connection.hpp>

#include <bnl/base/error.hpp>
#include <bnl/http3/error.hpp>
#include <bnl/util/error.hpp>

namespace bnl {
namespace http3 {
namespace client {

connection::connection(const log::api *logger)
  : control_{ client::stream::control::sender(logger),
              client::stream::control::receiver(logger) }
  , logger_(logger)
{}

base::result<quic::event>
connection::send() noexcept
{
  client::stream::control::sender &control = control_.first;

  {
    base::result<quic::event> event = control.send();
    if (event != base::error::idle) {
      return event;
    }
  }

  for (auto &entry : requests_) {
    client::stream::request::sender &sender = entry.second.first;

    if (sender.finished()) {
      continue;
    }

    base::result<quic::event> event = sender.send();
    if (event != base::error::idle) {
      if (event) {
        client::stream::request::receiver &receiver = entry.second.second;

        if (receiver.closed()) {
          TRY(receiver.start());
        }
      }

      return event;
    }
  }

  THROW(base::error::idle);
}

std::error_code
connection::recv(quic::event event, event::handler handler)
{
  client::stream::control::receiver &control = control_.second;

  if (event.id == control.id()) {
    auto control_handler = [this, &handler](http3::event event) {
      switch (event) {
        case event::type::settings:
          settings_.remote = event.settings;
          break;
        default:
          break;
      }

      return handler(std::move(event));
    };

    control.recv(std::move(event), control_handler);

    return {};
  }

  auto match = requests_.find(event.id);
  // TODO: Better error
  CHECK(match != requests_.end(), error::internal_error);

  client::stream::request::receiver &request = match->second.second;

  uint64_t id = event.id;
  TRY(request.recv(std::move(event), handler));

  if (request.finished()) {
    requests_.erase(id);
  }

  return {};
}

uint64_t
connection::request()
{
  uint64_t id = next_stream_id_;

  client::stream::request::sender sender(id, logger_);
  client::stream::request::receiver receiver(id, logger_);

  request_t request = std::make_pair(std::move(sender), std::move(receiver));
  requests_.insert(std::make_pair(id, std::move(request)));

  next_stream_id_ += 4;

  return id;
}

std::error_code
connection::header(uint64_t id, header_view header)
{
  auto match = requests_.find(id);
  CHECK(match != requests_.end(), error::stream_closed);

  client::stream::request::sender &sender = match->second.first;

  return sender.header(header);
}

std::error_code
connection::body(uint64_t id, base::buffer body)
{
  auto match = requests_.find(id);
  CHECK(match != requests_.end(), error::stream_closed);

  client::stream::request::sender &sender = match->second.first;

  return sender.body(std::move(body));
}

std::error_code
connection::start(uint64_t id) noexcept
{
  auto match = requests_.find(id);
  CHECK(match != requests_.end(), error::stream_closed);

  client::stream::request::sender &sender = match->second.first;

  return sender.start();
}

std::error_code
connection::fin(uint64_t id) noexcept
{
  auto match = requests_.find(id);
  CHECK(match != requests_.end(), error::stream_closed);

  client::stream::request::sender &sender = match->second.first;

  return sender.fin();
}

}
}
}