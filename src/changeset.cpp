#include "changeset.hpp"
#include "logger.hpp"
#include "http.hpp"
#include <boost/format.hpp>

using std::string;
using boost::format;

changeset::changeset(bool dp, const string &dn, long int id)
  : data_public(dp), display_name(dn), user_id(id) {
}

changeset *
fetch_changeset(pqxx::transaction_base &w, long int id) {
  pqxx::result res = w.exec("select u.data_public, u.display_name, u.id from users u "
			    "join changesets c on u.id=c.user_id where c.id=" + pqxx::to_string(id));

  // although the above query should always return one row, it might
  // happen that we get a weird changeset ID from somewhere, or the
  // FK constraints might have failed. in this situation all we can
  // really do is whine loudly and bail.
  if (res.empty()) {
     logger::message(format("ERROR: Request for user data associated with changeset "
                            "%1% failed: returned %2% rows.") % id % res.size());
     throw http::server_error((format("Possible database inconsistency with changeset %1%.") % id).str());
  }

  return new changeset(res[0][0].as<bool>(), res[0][1].as<string>(), res[0][2].as<long int>());
}

