#ifndef API06_HANDLER_UTILS_HPP
#define API06_HANDLER_UTILS_HPP

#include "cgimap/types.hpp"
#include "cgimap/request.hpp"
#include "cgimap/api06/id_version.hpp"
#include <vector>
#include <string_view>

namespace api06 {

std::vector<id_version> parse_id_list_params(request &req,
                                             std::string_view param_name);
}

#endif /* API06_HANDLER_UTILS_HPP */
