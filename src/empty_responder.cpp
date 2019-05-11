#include "cgimap/config.hpp"
#include "cgimap/empty_responder.hpp"

using std::list;
using std::shared_ptr;

empty_responder::empty_responder(mime::type mt)
    : responder(mt) {}

empty_responder::~empty_responder() = default;

list<mime::type> empty_responder::types_available() const {
  list<mime::type> types;
  types.push_back(mime::text_plain);
  return types;
}

void empty_responder::add_response_header(const std::string &line) {
  extra_headers << line << "\r\n";
}

std::string empty_responder::extra_response_headers() const {
  return extra_headers.str();
}

void empty_responder::write(std::shared_ptr<output_formatter> f,
                     const std::string &generator,
                     const std::chrono::system_clock::time_point &now)
{

}

