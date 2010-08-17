#include "routes.hpp"
#include "handler.hpp"
#include "map_handler.hpp"

using std::auto_ptr;

handler_ptr_t
route_request(FCGX_Request &request) {
  return auto_ptr<handler>(new map_handler(request));
}
