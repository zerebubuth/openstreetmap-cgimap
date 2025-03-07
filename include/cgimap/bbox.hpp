/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef BBOX_HPP
#define BBOX_HPP

#include <string>

/**
 * Container for a simple lat/lon bounding box.
 */
struct bbox {
  double minlat{0.0}, minlon{0.0}, maxlat{0.0}, maxlon{0.0};

  bbox(double minlat_, double minlon_, double maxlat_, double maxlon_);

  constexpr bbox() = default;

  bool operator==(const bbox &) const = default;

  /**
   * Attempt to parse a bounding box from a comma-separated string
   * of coordinates. Returns true if parsing was succesful and the
   * parameters have overwritten those in this instance.
   */
  bool parse(const std::string &s);

  /**
   * Reduce or increate the coordinates to ensure that they are all
   * valid lat/lon values.
   */
  void clip_to_world();

  /**
   * Returns true if this instance is a valid bounding box, i.e: the
   * coordinates are in the correct order and don't seem to be too
   * large or small.
   */
  bool valid() const;

  /**
   * The area of this bounding box in square degrees.
   */
  double area() const;
};

#endif /* BBOX_HPP */
