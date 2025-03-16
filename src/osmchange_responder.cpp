/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/logger.hpp"
#include "cgimap/osmchange_responder.hpp"

#include <chrono>
#include <fmt/core.h>
#include <algorithm>


namespace {

struct element {
  struct lonlat { double m_lon, m_lat; };

  element_type m_type;
  element_info m_info;
  tags_t m_tags;

  // only one of these will be non-empty depending on m_type
  lonlat m_lonlat;
  nodes_t m_nds;
  members_t m_members;
};


std::strong_ordering operator<=>(const element &a, const element &b) {

  if (auto result = a.m_info.timestamp <=> b.m_info.timestamp; result != 0) {
    return result;
  }

  if (auto result = a.m_info.version <=> b.m_info.version; result != 0) {
    return result;
  }

  if (auto result = a.m_type <=> b.m_type; result != 0) {
    return result;
  }

  return a.m_info.id <=> b.m_info.id;
}

struct sorting_formatter : public output_formatter {

  ~sorting_formatter() override = default;

  // LCOV_EXCL_START

  [[nodiscard]] mime::type mime_type() const override {
    throw std::runtime_error("sorting_formatter::mime_type unimplemented");
  }

  void start_document(
    const std::string &,
    const std::string &) override {

    throw std::runtime_error("sorting_formatter::start_document unimplemented");
  }

  void end_document() override {
    throw std::runtime_error("sorting_formatter::end_document unimplemented");
  }

  void error(const std::exception &) override {
    throw std::runtime_error("sorting_formatter::error unimplemented");
  }

  void write_bounds(const bbox &) override {
    throw std::runtime_error("sorting_formatter::write_bounds unimplemented");
  }

  void start_element() override {
    throw std::runtime_error("sorting_formatter::start_element unimplemented");
  }

  void end_element() override {
    throw std::runtime_error("sorting_formatter::end_element unimplemented");
  }

  void start_changeset(bool) override {
    throw std::runtime_error("sorting_formatter::start_changeset unimplemented");
  }

  void end_changeset(bool) override {
    throw std::runtime_error("sorting_formatter::end_changeset unimplemented");
  }

  // LCOV_EXCL_STOP

  void write_node(
    const element_info &elem,
    double lon, double lat,
    const tags_t &tags) override {

    element node{
      element_type::node, elem, tags, element::lonlat{lon, lat},
        nodes_t(), members_t()};

    m_elements.emplace_back(std::move(node));
  }

  void write_way(
    const element_info &elem,
    const nodes_t &nodes,
    const tags_t &tags) override {

    element way{
      element_type::way, elem, tags, element::lonlat{},
        nodes, members_t()};

    m_elements.emplace_back(std::move(way));
  }

  void write_relation(
    const element_info &elem,
    const members_t &members,
    const tags_t &tags) override {

    element rel{
      element_type::relation, elem, tags, element::lonlat{},
        nodes_t(), members};

    m_elements.emplace_back(std::move(rel));
  }

  // LCOV_EXCL_START

  void write_changeset(
    const changeset_info &,
    const tags_t &,
    bool,
    const comments_t &,
    const std::chrono::system_clock::time_point &) override {

    throw std::runtime_error("sorting_formatter::write_changeset unimplemented");
  }

  void write_diffresult_create_modify(const element_type,
				      const osm_nwr_signed_id_t,
				      const osm_nwr_id_t,
				      const osm_version_t) override {

    throw std::runtime_error("sorting_formatter::write_diffresult_create_modify unimplemented");
  }


  void write_diffresult_delete(const element_type,
                               const osm_nwr_signed_id_t) override {
    throw std::runtime_error("sorting_formatter::write_diffresult_delete unimplemented");
  }

  void flush() override {
    throw std::runtime_error("sorting_formatter::flush unimplemented");
  }

  // write an error to the output stream
  void error(const std::string &) override {
    throw std::runtime_error("sorting_formatter::error unimplemented");
  }

  void start_action(action_type) override {
    // this shouldn't be called here, as the things which call this don't have
    // actions - they're added by this code.
    throw std::runtime_error("Unexpected call to start_action.");
  }

  void end_action(action_type) override {
    // this shouldn't be called here, as the things which call this don't have
    // actions - they're added by this code.
    throw std::runtime_error("Unexpected call to end_action.");
  }

    // LCOV_EXCL_STOP

  void write(output_formatter &fmt) {
    std::sort(m_elements.begin(), m_elements.end());
    for (const auto &e : m_elements) {
      if (e.m_info.version == 1) {
        fmt.start_action(action_type::create);
        write_element(e, fmt);
        fmt.end_action(action_type::create);

      } else if (e.m_info.visible) {
        fmt.start_action(action_type::modify);
        write_element(e, fmt);
        fmt.end_action(action_type::modify);

      } else {
        fmt.start_action(action_type::del);
        write_element(e, fmt);
        fmt.end_action(action_type::del);
      }
    }
  }

private:
  std::vector<element> m_elements;

  void write_element(const element &e, output_formatter &fmt) {
    switch (e.m_type) {
    case element_type::node:
      fmt.write_node(e.m_info, e.m_lonlat.m_lon, e.m_lonlat.m_lat, e.m_tags);
      break;
    case element_type::way:
      fmt.write_way(e.m_info, e.m_nds, e.m_tags);
      break;
    case element_type::relation:
      fmt.write_relation(e.m_info, e.m_members, e.m_tags);
      break;
    case element_type::changeset:
      break;
    }
  }
};

} // anonymous namespace

osmchange_responder::osmchange_responder(mime::type mt, data_selection &s)
  : osm_responder(mt, {}),
    sel(s) {
}

std::vector<mime::type> osmchange_responder::types_available() const {
  return {mime::type::application_xml};
}

void osmchange_responder::write(output_formatter& fmt,
                                const std::string &generator,
                                const std::chrono::system_clock::time_point &now) {

  fmt.start_document(generator, "osmChange");
  try {
    sorting_formatter sorter;

    sel.write_nodes(sorter);
    sel.write_ways(sorter);
    sel.write_relations(sorter);

    sorter.write(fmt);

  } catch (const std::exception &e) {
    logger::message(fmt::format("Caught error in osmchange_responder: {}",
                          e.what()));
    fmt.error(e);
  }

  fmt.end_document();
}
