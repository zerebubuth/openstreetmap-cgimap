/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

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

  changeset() = default;

  changeset(bool dp, const std::string &dn, osm_user_id_t id);
};



#endif /* CHANGESET_HPP */
