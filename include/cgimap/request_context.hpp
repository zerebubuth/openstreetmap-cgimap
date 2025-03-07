/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef REQUEST_CONTEXT_HPP
#define REQUEST_CONTEXT_HPP

#include "cgimap/types.hpp"

#include <optional>
#include <set>

struct request;

struct UserInfo
{
    osm_user_id_t id = {};
    std::set<osm_user_role_t> user_roles = {};
    bool allow_api_write = false;
    bool has_role(osm_user_role_t role) const { return user_roles.count(role) > 0; }
};

struct RequestContext
{
    request& req;
    std::optional<UserInfo> user = {};

    bool is_moderator() const { return user && user->has_role(osm_user_role_t::moderator); }
};

#endif /* REQUEST_CONTEXT_HPP */
