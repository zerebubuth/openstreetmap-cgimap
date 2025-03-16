/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/output_formatter.hpp"
#include "test_formatter.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>


namespace {

bool equal_tags(const tags_t &a, const tags_t &b) {
  if (a.size() != b.size()) { return false; }
  auto sorted_a = a;
  auto sorted_b = b;
  std::ranges::sort(sorted_a);
  std::ranges::sort(sorted_b);
  return (sorted_a == sorted_b);
}

} // anonymous namespace

test_formatter::node_t::node_t(const element_info &elem_, double lon_, double lat_,
                               const tags_t &tags_)
  : elem(elem_), lon(lon_), lat(lat_), tags(tags_) {}

bool test_formatter::node_t::operator==(const node_t &other) const {
  return elem == other.elem &&
         lon == other.lon &&
         lat == other.lat &&
         equal_tags(tags, other.tags);
}

test_formatter::way_t::way_t(const element_info &elem_, const nodes_t &nodes_,
                             const tags_t &tags_)
  : elem(elem_), nodes(nodes_), tags(tags_) {}

bool test_formatter::way_t::operator==(const way_t &other) const {
  return elem == other.elem &&
         nodes == other.nodes &&
         equal_tags(tags, other.tags);
}

test_formatter::relation_t::relation_t(const element_info &elem_,
                                       const members_t &members_,
                                       const tags_t &tags_)
  : elem(elem_), members(members_), tags(tags_) {}

bool test_formatter::relation_t::operator==(const relation_t &other) const {
  return elem == other.elem &&
         members == other.members &&
         equal_tags(tags, other.tags);
}

test_formatter::changeset_t::changeset_t(const changeset_info &info,
                                         const tags_t &tags,
                                         bool include_comments,
                                         const comments_t &comments,
                                         const std::chrono::system_clock::time_point &time)
  : m_info(info)
  , m_tags(tags)
  , m_include_comments(include_comments)
  , m_comments(comments)
  , m_time(time) {
}

bool test_formatter::changeset_t::operator==(const changeset_t &other) const {
  return equal_tags(other.m_tags, m_tags) &&
         m_include_comments == other.m_include_comments &&
         m_info == other.m_info &&
         m_time == other.m_time &&
         (!m_include_comments || m_comments == other.m_comments);
}

// LCOV_EXCL_START

mime::type test_formatter::mime_type() const {
  throw std::runtime_error("Unimplemented");
}

void test_formatter::start_document(
  const std::string &generator, const std::string &root_name) {
}

void test_formatter::end_document() {
}

void test_formatter::write_bounds(const bbox &bounds) {
}

void test_formatter::start_element() {
}

void test_formatter::end_element() {
}

void test_formatter::start_changeset(bool) {
}

void test_formatter::end_changeset(bool) {
}

void test_formatter::start_action(action_type type) {
}

void test_formatter::end_action(action_type type) {
}

// LCOV_EXCL_STOP

void test_formatter::write_node(const element_info &elem, double lon, double lat,
                                const tags_t &tags) {
  m_nodes.emplace_back(elem, lon, lat, tags);
}
void test_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                               const tags_t &tags) {
  m_ways.emplace_back(elem, nodes, tags);
}

void test_formatter::write_relation(const element_info &elem,
                                    const members_t &members, const tags_t &tags) {
  m_relations.emplace_back(elem, members, tags);
}

void test_formatter::write_changeset(const changeset_info &elem, const tags_t &tags,
                                     bool include_comments, const comments_t &comments,
                                     const std::chrono::system_clock::time_point &time) {
  m_changesets.emplace_back(elem, tags, include_comments, comments, time);
}

void test_formatter::write_diffresult_create_modify(const element_type elem,
                                            const osm_nwr_signed_id_t old_id,
                                            const osm_nwr_id_t new_id,
                                            const osm_version_t new_version) {

}

void test_formatter::write_diffresult_delete(const element_type elem,
                                     const osm_nwr_signed_id_t old_id) {

}

void test_formatter::flush() {}

void test_formatter::error(const std::exception &e) {
  throw e;
}

void test_formatter::error(const std::string &str) {
  throw std::runtime_error(str);
}
