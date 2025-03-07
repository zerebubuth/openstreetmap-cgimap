/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSMCHANGE_RESPONDER_HPP
#define OSMCHANGE_RESPONDER_HPP

#include "cgimap/osm_responder.hpp"

class data_selection;

/**
 * utility class - inherit from this when implementing something that responds
 * with an osmChange (a.k.a "diff") document.
 */
class osmchange_responder : public osm_responder {
public:
  // construct, passing the mime type down to the responder.
  // optional bounds are stored at this level, but available to derived classes.
  osmchange_responder(mime::type, data_selection &s);

  ~osmchange_responder() override = default;

  // lists the standard types that OSM format can respond in, currently only XML,
  // as the osmChange format is undefined for JSON
  std::vector<mime::type> types_available() const override;

  // takes the stuff in the tmp_nodes/ways/relations tables and sorts them by
  // timestamp, then wraps them in <create>/<modify>/<delete> to create an
  // approximation of a diff. the reliance on timestamp means it's entirely
  // likely that some documents may be poorly formed.
  void write(output_formatter& f,
             const std::string &generator,
             const std::chrono::system_clock::time_point &now) override;

protected:
  // selection of elements to be written out
  data_selection& sel;
};

#endif /* OSMCHANGE_RESPONDER_HPP */
