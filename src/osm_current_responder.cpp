/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/logger.hpp"
#include "cgimap/osm_current_responder.hpp"

#include <fmt/core.h>

#include <optional>


osm_current_responder::osm_current_responder(mime::type mt, 
                                             data_selection &s,
                                             std::optional<bbox> b)
    : osm_responder(mt, b), 
      sel(s) {}


void osm_current_responder::write(output_formatter& fmt,
                                  const std::string &generator,
                                  const std::chrono::system_clock::time_point &now) {


  try {
    fmt.start_document(generator, "osm");
    if (bounds) {
      fmt.write_bounds(*bounds);
    }

    fmt.start_element();
    sel.write_nodes(fmt);      // write all selected nodes
    sel.write_ways(fmt);       // all selected ways
    sel.write_relations(fmt);  // all selected relations
    fmt.end_element();

  } catch (const std::exception &e) {
    logger::message(fmt::format("Caught error in osm_current_responder: {}",
                      e.what()));
    fmt.error(e);
  }

  fmt.end_document();
}
