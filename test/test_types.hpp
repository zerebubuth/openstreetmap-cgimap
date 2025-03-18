/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEST_TEST_TYPES_HPP
#define TEST_TEST_TYPES_HPP

#include <set>
#include "cgimap/types.hpp"

struct oauth2_token_detail_t
{
  bool expired;
  bool revoked;
  bool api_write;
  osm_user_id_t user_id;
};

using oauth2_tokens = std::map<std::string, oauth2_token_detail_t>;

using user_roles_t = std::map<osm_user_id_t, std::set<osm_user_role_t> >;

#endif