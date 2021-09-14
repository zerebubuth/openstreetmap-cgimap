#ifndef OAUTH2_HPP
#define OAUTH2_HPP

#include <memory>
#include <string>

#include "cgimap/data_selection.hpp"
#include "cgimap/http.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/request_helpers.hpp"

namespace oauth2 {
  boost::optional<osm_user_id_t> validate_bearer_token(request &req, std::shared_ptr<oauth::store>& store, bool& allow_api_write);
}

#endif
