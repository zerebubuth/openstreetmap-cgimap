#ifndef BBOX_HPP
#define BBOX_HPP

#include <string>

/**
 * Container for a simple lat/lon bounding box.
 */
struct bbox {
  double minlat, minlon, maxlat, maxlon;

  bbox(double minlat_, double minlon_, double maxlat_, double maxlon_);

  bbox();

  bool operator==(const bbox &) const;

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
