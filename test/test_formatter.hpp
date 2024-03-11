/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEST_TEST_FORMATTER
#define TEST_TEST_FORMATTER

#include "cgimap/output_formatter.hpp"

#include <fmt/core.h>

struct test_formatter : public output_formatter {
  struct node_t {
    node_t(const element_info &elem_, double lon_, double lat_,
           const tags_t &tags_);

    element_info elem;
    double lon, lat;
    tags_t tags;

    inline bool operator!=(const node_t &other) const {
      return !operator==(other);
    }

    bool operator==(const node_t &other) const;
  };

  struct way_t {
    way_t(const element_info &elem_, const nodes_t &nodes_,
          const tags_t &tags_);

    element_info elem;
    nodes_t nodes;
    tags_t tags;

    inline bool operator!=(const way_t &other) const {
      return !operator==(other);
    }

    bool operator==(const way_t &other) const;
  };

  struct relation_t {
    relation_t(const element_info &elem_, const members_t &members_,
               const tags_t &tags_);

    element_info elem;
    members_t members;
    tags_t tags;

    inline bool operator!=(const relation_t &other) const {
      return !operator==(other);
    }

    bool operator==(const relation_t &other) const;
  };

  struct changeset_t {
    changeset_info m_info;
    tags_t m_tags;
    bool m_include_comments;
    comments_t m_comments;
    std::chrono::system_clock::time_point m_time;

    changeset_t(const changeset_info &info,
                const tags_t &tags,
                bool include_comments,
                const comments_t &comments,
                const std::chrono::system_clock::time_point &time);

    inline bool operator!=(const changeset_t &other) const {
      return !operator==(other);
    }

    bool operator==(const changeset_t &other) const;
  };

  std::vector<changeset_t> m_changesets;
  std::vector<node_t> m_nodes;
  std::vector<way_t> m_ways;
  std::vector<relation_t> m_relations;

  ~test_formatter() override = default;
  mime::type mime_type() const override;
  void start_document(const std::string &generator, const std::string &root_name) override;
  void end_document() override;
  void write_bounds(const bbox &bounds) override;
  void start_element_type(element_type type) override;
  void end_element_type(element_type type) override;
  void start_action(action_type type) override;
  void end_action(action_type type) override;
  void write_node(const element_info &elem, double lon, double lat,
                  const tags_t &tags) override;
  void write_way(const element_info &elem, const nodes_t &nodes,
                 const tags_t &tags) override;
  void write_relation(const element_info &elem,
                      const members_t &members, const tags_t &tags) override;
  void write_changeset(const changeset_info &elem, const tags_t &tags,
                       bool include_comments, const comments_t &comments,
                       const std::chrono::system_clock::time_point &time) override;

  void write_diffresult_create_modify(const element_type elem,
                                              const osm_nwr_signed_id_t old_id,
                                              const osm_nwr_id_t new_id,
                                              const osm_version_t new_version) override;

  void write_diffresult_delete(const element_type elem,
                                       const osm_nwr_signed_id_t old_id) override;

  void flush() override;

  void error(const std::exception &e) override;
  void error(const std::string &str) override;
};


std::ostream &operator<<(std::ostream &out, const element_info &elem);
std::ostream &operator<<(std::ostream &out, const test_formatter::node_t &n);
std::ostream &operator<<(std::ostream &out, const test_formatter::changeset_t &n);
std::ostream &operator<<(std::ostream &out, const test_formatter::way_t &w);
std::ostream &operator<<(std::ostream &out, const member_info &m);
std::ostream &operator<<(std::ostream &out, const test_formatter::relation_t &r);


template <> struct fmt::formatter<test_formatter::node_t> {
  template <typename FormatContext>
  auto format(const test_formatter::node_t& n, FormatContext& ctx) -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    std::ostringstream ostr;
    ostr << n;
    return format_to(ctx.out(), "{}", ostr.str());
  }
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
};

template <> struct fmt::formatter<test_formatter::way_t> {
  template <typename FormatContext>
  auto format(const test_formatter::way_t& w, FormatContext& ctx) -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    std::ostringstream ostr;
    ostr << w;
    return format_to(ctx.out(), "{}", ostr.str());
  }

  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
};

template <> struct fmt::formatter<test_formatter::relation_t> {
  template <typename FormatContext>
  auto format(const test_formatter::relation_t& r, FormatContext& ctx) -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    std::ostringstream ostr;
    ostr << r;
    return format_to(ctx.out(), "{}", ostr.str());
  }
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
};

template <> struct fmt::formatter<test_formatter::changeset_t> {
  template <typename FormatContext>
  auto format(const test_formatter::changeset_t& cs, FormatContext& ctx) -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    std::ostringstream ostr;
    ostr << cs;
    return format_to(ctx.out(), "{}", ostr.str());
  }
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
};

#endif /* TEST_TEST_FORMATTER */
