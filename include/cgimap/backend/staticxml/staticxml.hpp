/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef STATICXML_BACKEND_HPP
#define STATICXML_BACKEND_HPP

#include "cgimap/backend.hpp"

#include <memory>

using user_roles_t = std::map<osm_user_id_t, std::set<osm_user_role_t> >;

std::unique_ptr<backend> make_staticxml_backend(user_roles_t = {});

#endif /* STATICXML_BACKEND_HPP */
