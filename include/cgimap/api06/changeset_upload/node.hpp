#ifndef NODE_HPP
#define NODE_HPP

#include "osmobject.hpp"

#include <boost/optional.hpp>
#include <iostream>

class Node : public OSMObject {

public:
  Node() : OSMObject(){};

  virtual ~Node(){};

  double lat() const { return *m_lat; }

  double lon() const { return *m_lon; }

  void set_lat(const char *lat) { set_lat(std::stod(lat)); }

  void set_lon(const char *lon) { set_lon(std::stod(lon)); }

  void set_lat(double lat) {
    if (lat < -90 || lat > 90)
      throw http::bad_request("Latitude outside of valid range");
    m_lat = lat;
  }

  void set_lon(double lon) {
    if (lon < -180 || lon > 180)
      throw http::bad_request("Longitude outside of valid range");
    m_lon = lon;
  }

  bool is_valid(operation op) const {

    switch (op) {

    case operation::op_delete:
      return (OSMObject::is_valid());
    default:
      return (OSMObject::is_valid() && m_lat && m_lon);
    }
  }

  std::string get_type_name() { return "Node"; }

private:
  boost::optional<double> m_lat;
  boost::optional<double> m_lon;
};

#endif
