#include "cgimap/config.hpp"
#include "cgimap/osm_responder.hpp"

using std::list;
using boost::shared_ptr;

osm_responder::osm_responder(mime::type mt, boost::optional<bbox> b)
    : responder(mt), bounds(b) {}

osm_responder::~osm_responder() {}

list<mime::type> osm_responder::types_available() const {
  list<mime::type> types;
  types.push_back(mime::text_xml);
#ifdef HAVE_YAJL
  types.push_back(mime::text_json);
#endif
  return types;
}

void osm_responder::add_response_header(const std::string &line) {
  extra_headers << line << "\r\n";
}

std::string osm_responder::extra_response_headers() const {
  return extra_headers.str();
}
