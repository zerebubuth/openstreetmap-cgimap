#ifndef OAUTH2_HPP
#define OAUTH2_HPP

#include <memory>
#include <string>

#include "cgimap/data_selection.hpp"
#include "cgimap/http.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/request_helpers.hpp"

namespace oauth2 {
  std::optional<osm_user_id_t> validate_bearer_token(const request &req, oauth::store* store, bool& allow_api_write);
}

#endif
