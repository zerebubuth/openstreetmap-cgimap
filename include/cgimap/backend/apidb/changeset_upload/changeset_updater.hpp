#ifndef APIDB_CHANGESET_UPDATER
#define APIDB_CHANGESET_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/changeset_updater.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

class ApiDB_Changeset_Updater : public Changeset_Updater {

public:
  ApiDB_Changeset_Updater(Transaction_Manager &_m,
                          osm_changeset_id_t _changeset, osm_user_id_t _uid);

  virtual ~ApiDB_Changeset_Updater();

  void lock_current_changeset();

  void update_changeset(const long num_new_changes, const bbox_t bbox);

private:
  Transaction_Manager &m;
  int cs_num_changes;
  osm_changeset_id_t changeset;
  osm_user_id_t uid;
  bbox_t cs_bbox;
};

#endif
