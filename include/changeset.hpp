#ifndef CHANGESET_HPP
#define CHANGESET_HPP

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
  long int user_id;

  changeset(bool dp, const std::string &dn, long int id);
};

changeset *fetch_changeset(pqxx::work &w, long int id);

#endif /* CHANGESET_HPP */
