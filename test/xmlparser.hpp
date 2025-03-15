/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEST_XMLPARSER_HPP
#define TEST_XMLPARSER_HPP

#include "cgimap/api06/id_version.hpp"
#include "cgimap/backend.hpp"
#include "cgimap/output_formatter.hpp"

#include <map>

using api06::id_version;

namespace xmlparser {

struct node {
  element_info m_info;
  double m_lon;
  double m_lat;
  tags_t m_tags;
};

struct way {
  element_info m_info;
  nodes_t m_nodes;
  tags_t m_tags;
};

struct relation {
  element_info m_info;
  members_t m_members;
  tags_t m_tags;
};

struct changeset {
  changeset_info m_info;
  tags_t m_tags;
  comments_t m_comments;
};

struct database {
  std::map<osm_changeset_id_t, changeset> m_changesets;
  std::map<id_version, node> m_nodes;
  std::map<id_version, way> m_ways;
  std::map<id_version, relation> m_relations;
};

} // namespace xmlparser

std::unique_ptr<xmlparser::database> parse_xml(const char *filename);
std::unique_ptr<xmlparser::database> parse_xml_from_string(const std::string &payload);

#endif