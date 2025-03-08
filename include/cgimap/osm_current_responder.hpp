/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSM_CURRENT_RESPONDER_HPP
#define OSM_CURRENT_RESPONDER_HPP

#include "cgimap/osm_responder.hpp"

#include <chrono>

class data_selection;

/**
 * utility class - inherit from this when implementing something
 * responding with OSM data from the current tables.
 */
class osm_current_responder : public osm_responder {
public:
  // construct, passing the mime type down to the responder.
  // optional bounds are stored at this level, but available to derived classes.
  osm_current_responder(mime::type, 
                        data_selection &s,
                        std::optional<bbox> bounds = {});

  // writes whatever is in the tmp_nodes/ways/relations tables to the given
  // formatter.
  void write(output_formatter& f,
             const std::string &generator,
             const std::chrono::system_clock::time_point &now) override;

protected:
  // current selection of elements to be written out
  data_selection& sel;
};

#endif /* OSM_CURRENT_RESPONDER_HPP */
