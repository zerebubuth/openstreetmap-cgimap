/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

 #ifndef API06_CHANGESET_UPLOAD_CHANGESET_STATS
 #define API06_CHANGESET_UPLOAD_CHANGESET_STATS


struct changeset_upload_stats {

  using value_t = uint64_t;

  struct element_stats {

    value_t num_create{ 0 };
    value_t num_modify{ 0 };
    value_t num_delete{ 0 };

    value_t get_total() const { return num_create + num_modify + num_delete; }
  };

  const element_stats node{};
  const element_stats way{};
  const element_stats relation{};

  value_t get_total() const {
    return node.get_total() + way.get_total() + relation.get_total();
  }
};

#endif