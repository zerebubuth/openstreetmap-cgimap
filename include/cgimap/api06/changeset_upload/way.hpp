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

  void add_way_node(const char *waynode) { add_way_node(std::stol(waynode)); }

  const std::vector<osm_nwr_signed_id_t> &nodes() const { return m_way_nodes; }

  bool is_valid(operation op) const {

    switch (op) {

    case operation::op_delete:
      return (OSMObject::is_valid());
    default:
      if (m_way_nodes.empty()) {
        throw http::precondition_failed(
            (boost::format("Way %1% must have at least one node") % (has_id() ? id() : 0))
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
