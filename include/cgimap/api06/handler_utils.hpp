#ifndef API06_HANDLER_UTILS_HPP
#define API06_HANDLER_UTILS_HPP

#include "cgimap/types.hpp"
#include "cgimap/request.hpp"
#include <list>
#include <string>

namespace api06 {

std::list<osm_id_t> parse_id_list_params(request &req,
                                         const std::string &param_name);
}

#endif /* API06_HANDLER_UTILS_HPP */
