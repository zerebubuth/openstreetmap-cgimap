/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSM_CHANGESET_RESPONDER_HPP
#define OSM_CHANGESET_RESPONDER_HPP

#include "cgimap/osm_responder.hpp"

#include <chrono>

class data_selection;

class osm_changeset_responder : public osm_responder {
public:
  // construct, passing the mime type down to the responder.
  // multi_selection flag can be set if we plan to fetch and print multiple changesets
  osm_changeset_responder(mime::type,
                          data_selection &s,
                          bool multi_selection);

  // writes whatever is in the selection to the formatter
  void write(output_formatter& f,
             const std::string &generator,
             const std::chrono::system_clock::time_point &now) override;

protected:
  // current selection of elements to be written out
  data_selection& sel;
  // do we want to select and print multiple changesets?
  bool multi_selection{false};
};

#endif /* OSM_CHANGESET_RESPONDER_HPP */
