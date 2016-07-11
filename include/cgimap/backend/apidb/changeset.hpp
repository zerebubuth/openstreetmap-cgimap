#ifndef CHANGESET_HPP
#define CHANGESET_HPP

#include "cgimap/types.hpp"
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

changeset *fetch_changeset(pqxx::transaction_base &w, osm_changeset_id_t id);

#endif /* CHANGESET_HPP */
