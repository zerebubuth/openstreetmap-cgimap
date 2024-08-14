/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSM_DIFFRESULT_RESPONDER_HPP
#define OSM_DIFFRESULT_RESPONDER_HPP

#include <vector>

#include "cgimap/osm_responder.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"


/**
 * utility class - inherit from this when implementing something that responds
 * with an diffResult document.
 */
class osm_diffresult_responder : public osm_responder {
public:

  explicit osm_diffresult_responder(mime::type);

  ~osm_diffresult_responder() override;


  void write(output_formatter& f,
             const std::string &generator,
             const std::chrono::system_clock::time_point &now) override;

protected:
  std::vector<api06::diffresult_t> m_diffresult;
};

#endif /* OSM_DIFFRESULT_RESPONDER_HPP */
