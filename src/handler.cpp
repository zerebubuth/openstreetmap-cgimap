#include "cgimap/handler.hpp"

responder::responder(mime::type mt) : mime_type(mt) {}

responder::~responder() = default;

bool responder::is_available(mime::type mt) const {
  std::list<mime::type> types = types_available();
  for (auto & itr : types) {
    if (itr == mt) {
      return true;
    }
  }
  return false;
}

mime::type responder::resource_type() const { return mime_type; }

std::string responder::extra_response_headers() const { return ""; }

handler::handler(
  mime::type default_type,
  http::method methods)
  : mime_type(default_type)
  , m_allowed_methods(methods) {}

handler::~handler() = default;

void handler::set_resource_type(mime::type mt) { mime_type = mt; }


payload_enabled_handler::payload_enabled_handler(
    mime::type default_type,
    http::method methods) : handler(default_type, methods) {}
