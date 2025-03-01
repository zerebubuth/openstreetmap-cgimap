/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef RELATION_HPP
#define RELATION_HPP

#include "cgimap/api06/changeset_upload/osmobject.hpp"
#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <optional>
#include <string>
#include <vector>

namespace api06 {

class RelationMember {

public:
  RelationMember() = default;

  RelationMember(const std::string &m_type, osm_nwr_signed_id_t m_ref, const std::string &m_role) :
    m_role(m_role),
    m_ref(m_ref),
    m_type(m_type) {}

  void set_type(const std::string &type) {

    if (iequals(type, "Node"))
      m_type = "Node";
    else if (iequals(type, "Way"))
      m_type = "Way";
    else if (iequals(type, "Relation"))
      m_type = "Relation";
    else
      throw payload_error(
          fmt::format("Invalid type {} in member relation", type));
  }

  void set_role(const std::string &role) {

    if (unicode_strlen(role) > 255) {
      throw payload_error(
          "Relation Role has more than 255 unicode characters");
    }

    m_role = role;
  }

  void set_ref(osm_nwr_signed_id_t ref) {

    if (ref == 0) {
      throw payload_error("Relation member 'ref' attribute may not be 0");
    }

    m_ref = ref;
  }

  void set_ref(const std::string &ref) {

    osm_nwr_signed_id_t _ref = 0;

    auto [_, ec] = std::from_chars(ref.data(), ref.data() + ref.size(), _ref);

    if (ec == std::errc()) {
      set_ref(_ref);
    }
    else if (ec == std::errc::invalid_argument)
      throw payload_error("Relation member 'ref' attribute is not numeric");
    else if (ec == std::errc::result_out_of_range)
      throw payload_error("Relation member 'ref' attribute value is too large");
    else
      throw payload_error("Unexpected parsing error");
  }

  bool is_valid() const {

    if (!m_type)
      throw payload_error("Missing 'type' attribute in Relation member");

    if (!m_ref)
      throw payload_error("Missing 'ref' attribute in Relation member");

    return (m_ref && m_type);
  }

  std::string type() const { return *m_type; }

  std::string role() const { return m_role; }

  osm_nwr_signed_id_t ref() const { return *m_ref; }

  bool operator==(const RelationMember &o) const = default;

private:
  std::string m_role;
  std::optional<osm_nwr_signed_id_t> m_ref;
  std::optional<std::string> m_type;
};

class Relation : public OSMObject {
public:
  Relation() = default;

  ~Relation() override = default;

  void add_members(std::vector<RelationMember>&& members) {
    for (auto& mbr : members)
      add_member(mbr);
  }

  void add_member(RelationMember &member) {
    if (!member.is_valid())
      throw payload_error(
          "Relation member does not include all mandatory fields");
    m_relation_member.emplace_back(member);
  }

  const std::vector<RelationMember> &members() const {
    return m_relation_member;
  }

  std::string get_type_name() const override { return "Relation"; }

  bool is_valid(operation op) const {

    if (op == operation::op_delete)
      return (is_valid());

    auto max_members = global_settings::get_relation_max_members();

    if (max_members && m_relation_member.size() > *max_members) {
      throw http::bad_request(
          fmt::format("You tried to add {:d} members to relation {:d}, however "
                      "only {:d} are allowed",
                      m_relation_member.size(), id(0),
                      *max_members));
    }

    return (is_valid());
  }

  bool operator==(const Relation &o) const {
    return (OSMObject::operator==(o) &&
            o.m_relation_member == m_relation_member);
  }

private:
  std::vector<RelationMember> m_relation_member;
  using OSMObject::is_valid;
};

} // namespace api06

#endif
