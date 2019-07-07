
#include "backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/changeset.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/http.hpp"
#include <map>
#include <boost/format.hpp>

using std::string;
using boost::format;

changeset::changeset(bool dp, const string &dn, osm_user_id_t id)
    : data_public(dp), display_name(dn), user_id(id) {}

std::map<osm_changeset_id_t, changeset *> fetch_changesets(pqxx::transaction_base &w, std::set< osm_changeset_id_t > ids) {

  std::map<osm_changeset_id_t, changeset*> result;

  if (ids.empty())
    return result;

  pqxx::result res =
      w.prepared("extract_changeset_userdetails")(ids).exec();

  for (const auto & r : res) {

    osm_changeset_id_t cs = r[0].as<int64_t>();

    // Multiple results for one changeset?
    if (result.find(cs) != result.end()) {
      logger::message(
          format("ERROR: Request for user data associated with changeset "
                 "%1% failed: returned multiple rows.") %
          cs);
      throw http::server_error(
          (format("Possible database inconsistency with changeset %1%.") % cs)
              .str());
    }

    int64_t user_id = r[3].as<int64_t>();
    // apidb instances external to OSM don't have access to anonymous
    // user information and so use an ID which isn't in use for any
    // other user to indicate this - generally 0 or negative.
    if (user_id <= 0) {
      result.insert(std::pair<osm_changeset_id_t, changeset*>(cs, new changeset(false, "", 0)));
    } else {
      result.insert(std::pair<osm_changeset_id_t, changeset*>(cs,
                          new changeset(r[1].as<bool>(), r[2].as<string>(), osm_user_id_t(user_id))));
    }
  }

  // although the above query should always return one row, it might
  // happen that we get a weird changeset ID from somewhere, or the
  // FK constraints might have failed. in this situation all we can
  // really do is whine loudly and bail.

  // Missing changeset in query result?
  for (const auto & id : ids) {
    if (result.find(id) == result.end()) {
      logger::message(
          format("ERROR: Request for user data associated with changeset "
                 "%1% failed: returned 0 rows.") % id);
      throw http::server_error(
          (format("Possible database inconsistency with changeset %1%.") % id).str());
    }
  }

  return result;
}

