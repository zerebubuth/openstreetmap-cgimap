/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEST_TEST_APIDB_IMPORTER_HPP
#define TEST_TEST_APIDB_IMPORTER_HPP

#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "xmlparser.hpp"
#include "test_types.hpp"

void populate_database(Transaction_Manager &m, const xmlparser::database &db,
                       const user_roles_t &user_roles,
                       const oauth2_tokens &oauth2_tokens);

#endif
