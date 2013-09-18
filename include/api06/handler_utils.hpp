#ifndef API06_HANDLER_UTILS_HPP
#define API06_HANDLER_UTILS_HPP

#include "types.hpp"
#include <list>
#include <string>
#include <fcgiapp.h>

namespace api06 {

std::list<osm_id_t> parse_id_list_params(FCGX_Request &request, const std::string &param_name);

}

#endif /* API06_HANDLER_UTILS_HPP */
