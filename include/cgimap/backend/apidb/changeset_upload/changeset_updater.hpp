#ifndef APIDB_CHANGESET_UPDATER
#define APIDB_CHANGESET_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/changeset_updater.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

class ApiDB_Changeset_Updater : public api06::Changeset_Updater {

public:
  ApiDB_Changeset_Updater(Transaction_Manager &_m,
                          osm_changeset_id_t _changeset, osm_user_id_t _uid);

  virtual ~ApiDB_Changeset_Updater();

  void lock_current_changeset(bool check_max_elements_limit);

  void update_changeset(const uint32_t num_new_changes, const bbox_t bbox);

  osm_changeset_id_t api_create_changeset(const std::map<std::string, std::string>&);

  void api_update_changeset(const std::map<std::string, std::string>&);

  void api_close_changeset();

private:
  void check_user_owns_changeset();
  void lock_cs(bool& is_closed, std::string& closed_at, std::string& current_time);
  void changeset_insert_subscriber ();
  void changeset_insert_tags (const std::map<std::string, std::string>& tags);
  void changeset_delete_tags ();
  void changeset_update_users_cs_count ();
  void changeset_insert_cs ();

  Transaction_Manager &m;
  uint32_t cs_num_changes;
  osm_changeset_id_t changeset;
  osm_user_id_t uid;
  bbox_t cs_bbox;
};

#endif
