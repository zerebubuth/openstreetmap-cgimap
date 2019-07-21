#ifndef CHANGESET_HPP
#define CHANGESET_HPP

#include "cgimap/types.hpp"
#include <map>
#include <set>
#include <string>
#include <pqxx/pqxx>

/**
 * This isn't really a changeset, its a user record. However, its probably
 * better to accept some duplication of effort and storage rather than add
 * another layer of indirection.
 */
struct changeset {
  bool data_public;
  std::string display_name;
  osm_user_id_t user_id;

  changeset(bool dp, const std::string &dn, osm_user_id_t id);
};

std::map<osm_changeset_id_t, changeset *> fetch_changesets(pqxx::transaction_base &w,  std::set<osm_changeset_id_t> id);


#endif /* CHANGESET_HPP */
