/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef DATA_UPDATE_HPP
#define DATA_UPDATE_HPP

#include "cgimap/types.hpp"

#include "cgimap/api06/changeset_upload/changeset_updater.hpp"
#include "cgimap/api06/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/relation_updater.hpp"
#include "cgimap/api06/changeset_upload/way_updater.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"

#include <memory>

struct RequestContext;
namespace api06 {
  class OSMChange_Tracking;
}


class data_update {
public:
  data_update() = default;

  virtual ~data_update() = default;

  data_update(const data_update&) = delete;
  data_update& operator=(const data_update&) = delete;

  data_update(data_update&&) = delete;
  data_update& operator=(data_update&&) = delete;

  virtual std::unique_ptr<api06::Changeset_Updater>
  get_changeset_updater(const RequestContext& ctx, osm_changeset_id_t _changeset) = 0;

  virtual std::unique_ptr<api06::Node_Updater>
  get_node_updater(const RequestContext& ctx, api06::OSMChange_Tracking &ct) = 0;

  virtual std::unique_ptr<api06::Way_Updater>
  get_way_updater(const RequestContext& ctx, api06::OSMChange_Tracking &ct) = 0;

  virtual std::unique_ptr<api06::Relation_Updater>
  get_relation_updater(const RequestContext& ctx, api06::OSMChange_Tracking &ct) = 0;

  virtual void
  commit() = 0;

  virtual bool is_api_write_disabled() const = 0;

  // get the current rate limit for changeset uploads for a given user id
  virtual uint32_t get_rate_limit(osm_user_id_t) = 0;

  // get the current maximum bounding box size for a given user id
  virtual uint64_t get_bbox_size_limit(osm_user_id_t uid) = 0;

  /**
   * factory for the creation of data updates. this abstracts away
   * the creation process of transactions, and allows some up-front
   * work to be done. for example, setting up prepared statements on
   * a database connection.
   */
  struct factory {
    virtual ~factory() = default;

    factory() = default;

    factory(const factory&) = delete;
    factory& operator=(const factory&) = delete;

    factory(factory&&) = delete;
    factory& operator=(factory&&) = delete;

    /// get a handle to a selection which can be used to build up
    /// a working set of data.
    virtual std::unique_ptr<data_update> make_data_update(Transaction_Owner_Base&) = 0;

    virtual std::unique_ptr<Transaction_Owner_Base> get_default_transaction() = 0;

    virtual std::unique_ptr<Transaction_Owner_Base> get_read_only_transaction() = 0;
  };
};


#endif /* DATA_UPDATE_HPP */
