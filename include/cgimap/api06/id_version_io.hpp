/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_ID_VERSION_IO_HPP
#define API06_ID_VERSION_IO_HPP

#include "cgimap/api06/id_version.hpp"
#include <iostream>

namespace api06 {

std::ostream &operator<<(std::ostream &out, const api06::id_version &idv);

} // namespace api06

#endif /* API06_ID_VERSION_IO_HPP */
