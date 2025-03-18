/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEST_TEST_CORE_HELPER_HPP
#define TEST_TEST_CORE_HELPER_HPP

#include "test_request.hpp"
#include "test_types.hpp"

#include <filesystem>
#include <istream>
#include <set>

namespace fs = std::filesystem;

void setup_request_headers(test_request &req, std::istream &in);
void check_response(std::istream &expected, std::istream &actual);
oauth2_tokens get_oauth2_tokens(const fs::path &oauth2_file);
user_roles_t get_user_roles(const fs::path &roles_file);

#endif