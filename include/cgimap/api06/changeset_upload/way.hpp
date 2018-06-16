#ifndef WAY_HPP
#define WAY_HPP

#include "osmobject.hpp"

#include <string>
#include <vector>

class Way : public OSMObject {

public:
  Way() : OSMObject(){};

  virtual ~Way(){};

  void add_way_node(osm_nwr_signed_id_t waynode) {
    m_way_nodes.emplace_back(waynode);
  }

  void add_way_node(const char *waynode) {

    osm_nwr_signed_id_t _waynode = 0;

    try {
	_waynode = std::stol(waynode);
    } catch (std::invalid_argument& e) {
	throw http::bad_request("Way node is not numeric");
    } catch (std::out_of_range& e) {
	throw http::bad_request("Way node value is too large");
    }

    if (_waynode == 0) {
	throw http::bad_request("Way node value may not be 0");
    }

    add_way_node(_waynode);
  }

  const std::vector<osm_nwr_signed_id_t> &nodes() const { return m_way_nodes; }

  bool is_valid(operation op) const {

    switch (op) {

    case operation::op_delete:
      return (OSMObject::is_valid());

    default:
      if (m_way_nodes.empty()) {
        throw http::precondition_failed(
            (boost::format("Way %1% must have at least one node") %
             (has_id() ? id() : 0))
                .str());
      }

      if (m_way_nodes.size() > WAY_MAX_NODES) {
        throw http::bad_request(
            (boost::format(
                 "You tried to add %1% nodes to way %2%, however only "
                 "%3% are allowed") %
             m_way_nodes.size() % (has_id() ? id() : 0) % WAY_MAX_NODES)
                .str());
      }

      return (OSMObject::is_valid());
    }
  }

  std::string get_type_name() { return "Way"; }

private:
  std::vector<osm_nwr_signed_id_t> m_way_nodes;
};

#endif
