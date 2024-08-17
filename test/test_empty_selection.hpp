/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2006-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */
 

#ifndef TEST_EMPTY_SELECTION
#define TEST_EMPTY_SELECTION

#include <memory>

#include "cgimap/data_selection.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"


class empty_data_selection : public data_selection {
public:

  ~empty_data_selection() override = default;

  void write_nodes(output_formatter &formatter) override {}
  void write_ways(output_formatter &formatter) override {}
  void write_relations(output_formatter &formatter) override {}
  void write_changesets(output_formatter &formatter,
                        const std::chrono::system_clock::time_point &now) override {}

  visibility_t check_node_visibility(osm_nwr_id_t id) override { return non_exist; }
  visibility_t check_way_visibility(osm_nwr_id_t id) override { return non_exist; }
  visibility_t check_relation_visibility(osm_nwr_id_t id) override { return non_exist; }

  int select_nodes(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_ways(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_relations(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_nodes_from_bbox(const bbox &bounds, int max_nodes) override { return 0; }
  void select_nodes_from_relations() override {}
  void select_ways_from_nodes() override {}
  void select_ways_from_relations() override {}
  void select_relations_from_ways() override {}
  void select_nodes_from_way_nodes() override {}
  void select_relations_from_nodes() override {}
  void select_relations_from_relations(bool drop_relations = false) override {}
  void select_relations_members_of_relations() override {}
  int select_changesets(const std::vector<osm_changeset_id_t> &) override { return 0; }
  void select_changeset_discussions() override {}
  void drop_nodes() override {}
  void drop_ways() override {}
  void drop_relations() override {}

  [[nodiscard]] bool supports_user_details() const override { return false; }
  bool is_user_blocked(const osm_user_id_t) override { return true; }
  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t id) override { return {}; }
  std::optional< osm_user_id_t > get_user_id_for_oauth2_token(
      const std::string &token_id, bool &expired, bool &revoked,
      bool &allow_api_write) override { return {}; }
  bool is_user_active(const osm_user_id_t id) override { return false; }

  int select_historical_nodes(const std::vector<osm_edition_t> &) override { return 0; }
  int select_nodes_with_history(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_historical_ways(const std::vector<osm_edition_t> &) override { return 0; }
  int select_ways_with_history(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_historical_relations(const std::vector<osm_edition_t> &) override { return 0; }
  int select_relations_with_history(const std::vector<osm_nwr_id_t> &) override { return 0; }
  void set_redactions_visible(bool) override {}
  int select_historical_by_changesets(const std::vector<osm_changeset_id_t> &) override { return 0; }


  struct factory : public data_selection::factory {
    ~factory() override = default;

    std::unique_ptr<data_selection> make_selection(Transaction_Owner_Base&) const override {
      return std::make_unique<empty_data_selection>();
    }
    std::unique_ptr<Transaction_Owner_Base> get_default_transaction() override {
      return std::make_unique<Transaction_Owner_Void>();
    }
  };
};


#endif
