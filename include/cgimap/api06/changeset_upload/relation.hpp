#ifndef RELATION_HPP
#define RELATION_HPP

#include "osmobject.hpp"

#include <boost/optional.hpp>
#include <boost/algorithm/string/predicate.hpp>

class RelationMember {

public:

	RelationMember() {};

	void set_type(const char* type) {

		std::string t = std::string(type);

		if (boost::iequals(t, "Node"))
			m_type = "Node";
		else if (boost::iequals(t, "Way"))
			m_type = "Way";
		else if (boost::iequals(t, "Relation"))
			m_type = "Relation";
		else
			throw std::runtime_error("Invalid type in member relation");
	}

	void set_role(const char* role) {
		m_role = std::string(role);
	}

	void set_ref(const char* ref) {
		m_ref = std::stol(ref);
	}

	bool is_valid() {

		return (m_role &&
				m_ref &&
				m_type);
	}

	std::string type() const {
		return *m_type;
	}

	std::string role() const {
		return *m_role;
	}

	osm_nwr_signed_id_t ref() const {
		return *m_ref;
	}

private:
	boost::optional< std::string >         m_role;
	boost::optional< osm_nwr_signed_id_t > m_ref;
	boost::optional< std::string >         m_type;
};


class Relation : public OSMObject
{
public:

	Relation() : OSMObject() {};

	void add_member(RelationMember& member) {
		if (!member.is_valid())
			throw std::runtime_error ("Relation member does not include all mandatory fields");
		m_relation_member.emplace_back(member);
	}

	const std::vector< RelationMember >& members() const
	{
		return m_relation_member;
	}

	bool is_valid() const {

		return (OSMObject::is_valid());
	}


private:
	std::vector< RelationMember > m_relation_member;

};

#endif
