#ifndef WAY_HPP
#define WAY_HPP

#include "osmobject.hpp"

#include <string>
#include <vector>

class Way : public OSMObject {

public:
  Way() : OSMObject(){};

  virtual ~Way() {};

  void add_way_node(osm_nwr_signed_id_t waynode) {
    m_way_nodes.emplace_back(waynode);
  }

  void add_way_node(const char *waynode) { add_way_node(std::stol(waynode)); }

  const std::vector<osm_nwr_signed_id_t> &nodes() const { return m_way_nodes; }

  std::string get_type_name() { return "Way"; }

private:
  std::vector<osm_nwr_signed_id_t> m_way_nodes;
};

#endif
