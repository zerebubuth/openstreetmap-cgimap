#include "changeset.hpp"

using std::string;

changeset::changeset(bool dp, const string &dn, long int id)
  : data_public(dp), display_name(dn), user_id(id) {
}

changeset *
fetch_changeset(pqxx::work &w, long int id) {
  pqxx::result res = w.exec("select u.data_public, u.display_name, u.id from users u "
			    "join changesets c on u.id=c.user_id where c.id=" + pqxx::to_string(id));
  
  return new changeset(res[0][0].as<bool>(), res[0][1].as<string>(), res[0][2].as<long int>());
}

