#ifndef NODE_HPP
#define NODE_HPP

#include "osmobject.hpp"

#include <boost/optional.hpp>
#include <iostream>

namespace api06 {

class Node : public OSMObject {

public:
  Node() : OSMObject() {};

  virtual ~Node() = default;

  double lat() const { return *m_lat; }

  double lon() const { return *m_lon; }

  void set_lat(const char *lat) {

    double _lat = -200.0;

    try {
      _lat = std::stod(lat);
    } catch (std::invalid_argument &e) {
      throw xml_error("Latitude is not numeric");
    } catch (std::out_of_range &e) {
      throw xml_error("Latitude value is too large");
    }

    set_lat(_lat);
  }

  void set_lon(const char *lon) {

    double _lon = -200.0;

    try {
      _lon = std::stod(lon);
    } catch (std::invalid_argument &e) {
      throw xml_error("Longitude is not numeric");
    } catch (std::out_of_range &e) {
      throw xml_error("Longitude value is too large");
    }

    set_lon(_lon);
  }

  void set_lat(double lat) {
    if (lat < -90 || lat > 90)
      throw xml_error("Latitude outside of valid range");
    m_lat = lat;
  }

  void set_lon(double lon) {
    if (lon < -180 || lon > 180)
      throw xml_error("Longitude outside of valid range");
    m_lon = lon;
  }

  bool is_valid(operation op) const {

    switch (op) {

    case operation::op_delete:
      return (is_valid());
    default:
      return (is_valid() && m_lat && m_lon);
    }
  }

  std::string get_type_name() { return "Node"; }

private:
  boost::optional<double> m_lat;
  boost::optional<double> m_lon;
  using OSMObject::is_valid;
};

} // namespace api06

#endif
