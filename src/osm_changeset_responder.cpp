/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/logger.hpp"
#include "cgimap/osm_changeset_responder.hpp"

#include <fmt/core.h>


osm_changeset_responder::osm_changeset_responder(mime::type mt,
                                                 data_selection &s,
                                                 bool multi_selection)
    : osm_responder(mt),
      sel(s),
      multi_selection(multi_selection){}


void osm_changeset_responder::write(output_formatter& fmt,
                                    const std::string &generator,
                                    const std::chrono::system_clock::time_point &now) {

  try {
    fmt.start_document(generator, "osm");

    fmt.start_changeset(multi_selection);

    // write changeset
    sel.write_changesets(fmt, now);

    fmt.end_changeset(multi_selection);

  } catch (const std::exception &e) {
    logger::message(fmt::format("Caught error in osm_changeset_responder: {}",
                      e.what()));
    fmt.error(e);
  }

  fmt.end_document();
}
