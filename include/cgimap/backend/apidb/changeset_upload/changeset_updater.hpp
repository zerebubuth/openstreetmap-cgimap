/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef APIDB_CHANGESET_UPDATER
#define APIDB_CHANGESET_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"
#include "cgimap/api06/changeset_upload/changeset_updater.hpp"

#include <cstdint>

struct RequestContext;
class Transaction_Manager;

class ApiDB_Changeset_Updater : public api06::Changeset_Updater {

public:
  ApiDB_Changeset_Updater(Transaction_Manager &_m,
                          const RequestContext& _req_ctx, 
                          osm_changeset_id_t _changeset);

  ~ApiDB_Changeset_Updater() override = default;

  void lock_current_changeset(bool check_max_elements_limit) override;

  void update_changeset(const uint32_t num_new_changes, const bbox_t bbox) override;

  bbox_t get_bbox() const override;

  osm_changeset_id_t api_create_changeset(const std::map<std::string, std::string>&) override;

  void api_update_changeset(const std::map<std::string, std::string>&) override;

  void api_close_changeset() override;

private:
  void check_user_owns_changeset();
  void lock_cs(bool& is_closed, std::string& closed_at, std::string& current_time);
  void changeset_insert_subscriber ();
  void changeset_insert_tags (const std::map<std::string, std::string>& tags);
  void changeset_delete_tags ();
  void changeset_update_users_cs_count ();
  void changeset_insert_cs ();

  Transaction_Manager &m;
  const RequestContext& req_ctx;
  uint32_t cs_num_changes{0};
  osm_changeset_id_t changeset;
  bbox_t cs_bbox{};
};

#endif
