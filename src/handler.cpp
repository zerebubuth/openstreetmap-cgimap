#include "cgimap/handler.hpp"

responder::responder(mime::type mt) : mime_type(mt) {}

responder::~responder() {}

bool responder::is_available(mime::type mt) const {
  std::list<mime::type> types = types_available();
  for (std::list<mime::type>::iterator itr = types.begin(); itr != types.end();
       ++itr) {
    if (*itr == mt) {
      return true;
    }
  }
  return false;
}

mime::type responder::resource_type() const { return mime_type; }

std::string responder::extra_response_headers() const { return ""; }

handler::handler(mime::type default_type) : mime_type(default_type) {}

handler::~handler() {}

void handler::set_resource_type(mime::type mt) { mime_type = mt; }
