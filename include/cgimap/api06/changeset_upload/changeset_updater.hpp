/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_CHANGESET_UPLOAD_CHANGESET_UPDATER
#define API06_CHANGESET_UPLOAD_CHANGESET_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <map>
#include <string>
#include <cstdint>

namespace api06 {

class Changeset_Updater {

public:
  virtual ~Changeset_Updater() = default;

  virtual void lock_current_changeset(bool check_max_elements_limit) = 0;

  virtual void update_changeset(uint32_t num_new_changes, bbox_t bbox) = 0;

  virtual bbox_t get_bbox() const = 0;

  virtual osm_changeset_id_t api_create_changeset(const std::map<std::string, std::string>&) = 0;

  virtual void api_update_changeset(const std::map<std::string, std::string>&) = 0;

  virtual void api_close_changeset() = 0;
};

} // namespace api06

#endif
