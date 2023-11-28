/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/id_version.hpp"
#include <iostream>

namespace api06 {

std::ostream &operator<<(std::ostream &out, const api06::id_version &idv) {
  out << idv.id;
  if (idv.version) {
    out << "v" << *idv.version;
  }
  return out;
}

} // namespace api06
