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

std::unique_ptr<backend> make_staticxml_backend();

#endif /* STATICXML_BACKEND_HPP */
