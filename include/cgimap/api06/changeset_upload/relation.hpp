/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef RELATION_HPP
#define RELATION_HPP

#include "cgimap/api06/changeset_upload/osmobject.hpp"
#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <optional>
#include <boost/algorithm/string/predicate.hpp>


namespace api06 {

class RelationMember {

public:
  RelationMember() = default;

  RelationMember(const std::string &m_type, osm_nwr_signed_id_t m_ref, const std::string &m_role) :
    m_role(m_role), 
    m_ref(m_ref), 
    m_type(m_type) {}

  void set_type(const std::string &type) {

    if (boost::iequals(type, "Node"))
      m_type = "Node";
    else if (boost::iequals(type, "Way"))
      m_type = "Way";
    else if (boost::iequals(type, "Relation"))
      m_type = "Relation";
    else
      throw xml_error(
          fmt::format("Invalid type {} in member relation", type));
  }

  void set_role(const std::string &role) {

    if (unicode_strlen(role) > 255) {
      throw xml_error(
          "Relation Role has more than 255 unicode characters");
    }

    m_role = role;
  }

  void set_ref(const std::string &ref) {

    osm_nwr_signed_id_t _ref = 0;

    try {
      _ref = std::stol(ref);
    } catch (std::invalid_argument &e) {
      throw xml_error("Relation member 'ref' attribute is not numeric");
    } catch (std::out_of_range &e) {
      throw xml_error(
          "Relation member 'ref' attribute value is too large");
    }

    if (_ref == 0) {
      throw xml_error("Relation member 'ref' attribute may not be 0");
    }

    m_ref = _ref;
  }

  bool is_valid() const {

    if (!m_type)
      throw xml_error("Missing 'type' attribute in Relation member");

    if (!m_ref)
      throw xml_error("Missing 'ref' attribute in Relation member");

    return (m_ref && m_type);
  }

  std::string type() const { return *m_type; }

  std::string role() const { return m_role; }

  osm_nwr_signed_id_t ref() const { return *m_ref; }

private:
  std::string m_role;
  std::optional<osm_nwr_signed_id_t> m_ref;
  std::optional<std::string> m_type;
};

class Relation : public OSMObject {
public:
  Relation() = default;

  ~Relation() override = default;

  void add_member(RelationMember &member) {
    if (!member.is_valid())
      throw xml_error(
          "Relation member does not include all mandatory fields");
    m_relation_member.emplace_back(member);
  }

  const std::vector<RelationMember> &members() const {
    return m_relation_member;
  }

  std::string get_type_name() override { return "Relation"; }

  bool is_valid(operation op) const {

    switch (op) {

    case operation::op_delete:
      return (is_valid());

    default:
      if ((global_settings::get_relation_max_members()) &&
	  m_relation_member.size() > *global_settings::get_relation_max_members()) {
        throw http::bad_request(
             fmt::format("You tried to add {:d} members to relation {:d}, however only {:d} are allowed",
        	m_relation_member.size(),
        	(has_id() ? id() : 0),
        	*global_settings::get_relation_max_members()));
      }

      return (is_valid());
    }
  }

private:
  std::vector<RelationMember> m_relation_member;
  using OSMObject::is_valid;
};

} // namespace api06

#endif
