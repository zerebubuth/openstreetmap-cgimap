/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSM_RESPONDER_HPP
#define OSM_RESPONDER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/bbox.hpp"

#include <optional>
#include <sstream>

/**
 * utility class - use this as a base class when the derived class is going to
 * respond in OSM format (i.e: nodes, ways and relations). this class will take
 * care of the write() and types_available() methods, allowing derived code to
 * be more concise.
 *
 * if you want a <bounds> element to be written, include the bounds constructor
 * argument. otherwise leave it out and it'll default to no bounds element.
 */
class osm_responder : public responder {
public:
  // construct, passing the mime type down to the responder.
  // optional bounds are stored at this level, but available to derived classes.
  explicit osm_responder(mime::type, std::optional<bbox> bounds = {});

  ~osm_responder() override = default;

  // lists the standard types that OSM format can respond in, currently XML and JSON
  std::vector<mime::type> types_available() const override;

  // quick hack to add headers to the response
  std::string extra_response_headers() const override;

protected:
  // optional bounds element - this is only for information and has no effect on
  // behaviour other than whether the bounds element gets written.
  std::optional<bbox> bounds;

  // quick hack to provide extra response headers like Content-Disposition.
  std::ostringstream extra_headers;

  // adds an extra response header.
  void add_response_header(const std::string &);
};

#endif /* OSM_RESPONDER_HPP */
