
#include "backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/changeset.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/http.hpp"
#include <map>
#include <fmt/core.h>

using std::string;


changeset::changeset(bool dp, const string &dn, osm_user_id_t id)
    : data_public(dp), display_name(dn), user_id(id) {}



