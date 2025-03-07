/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/changeset.hpp"

#include <fmt/core.h>


changeset::changeset(bool dp, const std::string &dn, osm_user_id_t id)
    : data_public(dp), display_name(dn), user_id(id) {}



