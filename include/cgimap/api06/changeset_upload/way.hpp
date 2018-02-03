#ifndef WAY_HPP
#define WAY_HPP

#include "osmobject.hpp"

#include <vector>

class Way : public OSMObject
{

public:

	Way() : OSMObject() {};

	void add_way_node(osm_nwr_signed_id_t waynode)
	{
		m_way_nodes.emplace_back(waynode);
	}

	void add_way_node(const char* waynode)
	{
        add_way_node(std::stol(waynode));
	}

	const std::vector< osm_nwr_signed_id_t > & nodes() const
	{
		return m_way_nodes;
	}

private:

	std::vector< osm_nwr_signed_id_t > m_way_nodes;

};

#endif
