/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "xmlparser.hpp"
#include "staticxml.hpp"
#include "cgimap/backend.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/api06/id_version.hpp"

#include <map>
#include <istream>
#include <ostream>
#include <ios>
#include <string_view>
#include <unordered_set>
#include <optional>

#include <fmt/core.h>
#include <libxml/parser.h>

namespace po = boost::program_options;

namespace {

template <typename T>
void write_element(const T &, output_formatter &formatter);

template <>
inline void write_element<node>(const node &n, output_formatter &formatter) {
  formatter.write_node(n.m_info, n.m_lon, n.m_lat, n.m_tags);
}

template <>
inline void write_element<way>(const way &w, output_formatter &formatter) {
  formatter.write_way(w.m_info, w.m_nodes, w.m_tags);
}

template <>
inline void write_element<relation>(const relation &r, output_formatter &formatter) {
  formatter.write_relation(r.m_info, r.m_members, r.m_tags);
}

struct static_data_selection : public data_selection {
  explicit static_data_selection(database& db) : static_data_selection(db, {}, {}) {}

  explicit static_data_selection(database& db, const user_roles_t& m_user_roles, const oauth2_tokens& m_oauth2_tokens)
  : m_db(db)
  , m_user_roles(m_user_roles)
  , m_oauth2_tokens(m_oauth2_tokens){}

  ~static_data_selection() override = default;

  void write_nodes(output_formatter &formatter) override {
    write_elements<node>(m_historic_nodes, m_nodes, formatter);
  }

  void write_ways(output_formatter &formatter) override {
    write_elements<way>(m_historic_ways, m_ways, formatter);
  }

  void write_relations(output_formatter &formatter) override {
    write_elements<relation>(m_historic_relations, m_relations, formatter);
  }

  void write_changesets(output_formatter &formatter,
                                const std::chrono::system_clock::time_point &now) override {
    for (osm_changeset_id_t id : m_changesets) {
      auto itr = m_db.m_changesets.find(id);
      if (itr != m_db.m_changesets.end()) {
        const changeset &c = itr->second;
        formatter.write_changeset(
          c.m_info, c.m_tags, m_include_changeset_comments,
          c.m_comments, now);
      }
    }
  }

  visibility_t check_node_visibility(osm_nwr_id_t id) override {
    return check_visibility<node>(id);
  }

  visibility_t check_way_visibility(osm_nwr_id_t id) override {
    return check_visibility<way>(id);
  }

  visibility_t check_relation_visibility(osm_nwr_id_t id) override {
    return check_visibility<relation>(id);
  }

  int select_nodes(const std::vector<osm_nwr_id_t> &ids) override {
    return select<node>(m_nodes, ids);
  }

  int select_ways(const std::vector<osm_nwr_id_t> &ids) override {
    return select<way>(m_ways, ids);
  }

  int select_relations(const std::vector<osm_nwr_id_t> &ids) override {
    return select<relation>(m_relations, ids);
  }

  int select_nodes_from_bbox(const bbox &bounds, int max_nodes) override {
    int selected = 0;
    const auto end = m_db.m_nodes.cend();
    for (auto itr = m_db.m_nodes.cbegin(); itr != end; ++itr) {
      auto next = itr; ++next;
      const node &n = itr->second;
      if ((next == end || next->second.m_info.id != n.m_info.id) &&
          (n.m_lon >= bounds.minlon) && (n.m_lon <= bounds.maxlon) &&
          (n.m_lat >= bounds.minlat) && (n.m_lat <= bounds.maxlat) &&
          (n.m_info.visible)) {
        m_nodes.insert(n.m_info.id);
        ++selected;

        if (selected > max_nodes) {
          break;
        }
      }
    }
    return selected;
  }

  void select_nodes_from_relations() override {
    for (osm_nwr_id_t id : m_relations) {
      auto r = find_current<relation>(id);
      if (r) {
        for (const member_info &m : r->get().m_members) {
          if (m.type == element_type::node) {
            m_nodes.insert(m.ref);
          }
        }
      }
    }
  }

  void select_ways_from_nodes() override {
    const auto end = m_db.m_ways.cend();
    for (auto itr = m_db.m_ways.cbegin(); itr != end; ++itr) {
      auto next = itr; ++next;
      const way &w = itr->second;
      if (next == end || next->second.m_info.id != w.m_info.id) {
        for (osm_nwr_id_t node_id : w.m_nodes) {
          if (m_nodes.contains(node_id)) {
            m_ways.insert(w.m_info.id);
            break;
          }
        }
      }
    }
  }

  void select_ways_from_relations() override {
    for (osm_nwr_id_t id : m_relations) {
      auto r = find_current<relation>(id);
      if (r) {
        for (const member_info &m : r->get().m_members) {
          if (m.type == element_type::way) {
            m_ways.insert(m.ref);
          }
        }
      }
    }
  }

  void select_relations_from_ways() override {
    const auto end = m_db.m_relations.cend();
    for (auto itr = m_db.m_relations.cbegin(); itr != end; ++itr) {
      auto next = itr; ++next;
      const relation &r = itr->second;
      if (next == end || next->second.m_info.id != r.m_info.id) {
        for (const member_info &m : r.m_members) {
          if (m.type == element_type::way && m_ways.contains(m.ref)) {
            m_relations.insert(r.m_info.id);
            break;
          }
        }
      }
    }
  }

  void select_nodes_from_way_nodes() override {
    for (osm_nwr_id_t id : m_ways) {
      auto w = find_current<way>(id);
      if (w) {
        m_nodes.insert(w->get().m_nodes.begin(), w->get().m_nodes.end());
      }
    }
  }

  void select_relations_from_nodes() override {
    for (auto const & [_, r] : m_db.m_relations) {
      for (const member_info &m : r.m_members) {
        if (m.type == element_type::node && m_nodes.contains(m.ref)) {
          m_relations.insert(r.m_info.id);
          break;
        }
      }
    }
  }

  void select_relations_from_relations(bool drop_relations = false) override {
    std::set<osm_nwr_id_t> tmp_relations;
    for (auto const & [_, r] : m_db.m_relations) {
      for (const member_info &m : r.m_members) {
        if (m.type == element_type::relation &&
            m_relations.contains(m.ref)) {
          tmp_relations.insert(r.m_info.id);
          break;
        }
      }
    }
    if (drop_relations)
      m_relations.clear();
    m_relations.insert(tmp_relations.begin(), tmp_relations.end());
  }

  void select_relations_members_of_relations() override {
    for (osm_nwr_id_t id : m_relations) {
      auto r = find_current<relation>(id);
      if (r) {
        for (const member_info &m : r->get().m_members) {
          if (m.type == element_type::relation) {
            m_relations.insert(m.ref);
          }
        }
      }
    }
  }

  int select_historical_nodes(const std::vector<osm_edition_t> &editions) override {
    return select_historical<node>(m_historic_nodes, editions);
  }

  int select_nodes_with_history(const std::vector<osm_nwr_id_t> &ids) override {
    return select_historical_all<node>(m_historic_nodes, ids);
  }

  int select_historical_ways(const std::vector<osm_edition_t> &editions) override  {
    return select_historical<way>(m_historic_ways, editions);
  }

  int select_ways_with_history(const std::vector<osm_nwr_id_t> &ids) override {
    return select_historical_all<way>(m_historic_ways, ids);
  }

  int select_historical_relations(const std::vector<osm_edition_t> &editions) override {
    return select_historical<relation>(m_historic_relations, editions);
  }

  int select_relations_with_history(const std::vector<osm_nwr_id_t> &ids) override {
    return select_historical_all<relation>(m_historic_relations, ids);
  }

  void set_redactions_visible(bool visible) override {
    m_redactions_visible = visible;
  }

  int select_historical_by_changesets(
    const std::vector<osm_changeset_id_t> &ids) override {

    std::unordered_set<osm_changeset_id_t> changesets(ids.begin(), ids.end());

    int selected = 0;
    selected += select_by_changesets<node>(m_historic_nodes, changesets);
    selected += select_by_changesets<way>(m_historic_ways, changesets);
    selected += select_by_changesets<relation>(m_historic_relations, changesets);

    return selected;
  }

  void drop_nodes() override {
    m_nodes.clear();
  }

  void drop_ways() override {
    m_ways.clear();
  }

  void drop_relations() override {
    m_relations.clear();
  }

  int select_changesets(const std::vector<osm_changeset_id_t> &ids) override {
    int selected = 0;
    for (osm_changeset_id_t id : ids) {
      if (m_db.m_changesets.contains(id)) {
        m_changesets.insert(id);
        ++selected;
      }
    }
    return selected;
  }

  void select_changeset_discussions() override {
    m_include_changeset_comments = true;
  }

  bool supports_user_details() const override { return false; }
  bool is_user_blocked(const osm_user_id_t) override { return true; }
  bool is_user_active(const osm_user_id_t) override { return true; }

  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t id) override {
    std::set<osm_user_role_t> roles;
    auto itr = m_user_roles.find(id);
    if (itr != m_user_roles.end()) {
      roles = itr->second;
    }
    return roles;
  }

  std::optional< osm_user_id_t > get_user_id_for_oauth2_token(
        const std::string &token_id, bool &expired, bool &revoked,
        bool &allow_api_write) override {

    auto itr = m_oauth2_tokens.find(token_id);
    if (itr != m_oauth2_tokens.end())
    {
      expired = itr->second.expired;
      revoked = itr->second.revoked;
      allow_api_write = itr->second.api_write;
      return itr->second.user_id;
    }

    expired = false;
    revoked = false;
    allow_api_write = false;
    return {};
  }

private:
  template <typename T>
  const std::map<id_version, T> &map_of() const;

  template <typename T>
  [[nodiscard]] std::optional<std::reference_wrapper<const T> > find_current(osm_nwr_id_t id) const {
    using element_map_t = std::map<id_version, T>;
    id_version idv(id);
    const element_map_t &m = map_of<T>();
    if (!m.empty()) {
      auto itr = m.upper_bound(idv);
      if (itr != m.begin()) {
        --itr;
        if (itr->first.id == id) {
          return std::optional< std::reference_wrapper<const T> >{itr->second};
        }
      }
    }
    return {};
  }

  template <typename T>
  std::optional<std::reference_wrapper<const T> > find(osm_edition_t edition) const {
    using element_map_t = std::map<id_version, T>;
    id_version idv(edition.first, edition.second);
    const element_map_t &m = map_of<T>();
    if (!m.empty()) {
      auto itr = m.find(idv);
      if (itr != m.end()) {
        return std::optional< std::reference_wrapper<const T> >{itr->second};
      }
    }
    return {};
  }

  template <typename T>
  void write_elements(const std::set<osm_edition_t> &historic_ids,
                      const std::set<osm_nwr_id_t> &current_ids,
                      output_formatter &formatter) const {
    std::set<osm_edition_t> editions = historic_ids;

    for (osm_nwr_id_t id : current_ids) {
      auto maybe = find_current<T>(id);
      if (maybe) {
        const T &t = *maybe;
        editions.insert(std::make_pair(id, t.m_info.version));
      }
    }

    for (osm_edition_t ed : editions) {
      auto maybe = find<T>(ed);
      if (maybe) {
        const T &t = *maybe;
        write_element(t, formatter);
      }
    }
  }

  template <typename T>
  visibility_t check_visibility(osm_nwr_id_t &id) {
    auto maybe = find_current<T>(id);
    if (maybe) {
      if (maybe->get().m_info.visible) {
        return data_selection::exists;
      } else {
        return data_selection::deleted;
      }
    }
    return data_selection::non_exist;
  }

  template <typename T>
  int select(std::set<osm_nwr_id_t> &found_ids,
             const std::vector<osm_nwr_id_t>& select_ids) const {
    int selected = 0;
    for (osm_nwr_id_t id : select_ids) {
      auto t = find_current<T>(id);
      if (t) {
        found_ids.insert(id);
        ++selected;
      }
    }
    return selected;
  }

  template <typename T>
  int select_historical(std::set<osm_edition_t> &found_eds,
                        const std::vector<osm_edition_t> &select_eds) const {
    int selected = 0;
    for (osm_edition_t ed : select_eds) {
      auto t = find<T>(ed);
      if (t) {
        auto is_redacted = bool(t->get().m_info.redaction);
        if (!is_redacted || m_redactions_visible) {
          found_eds.insert(ed);
          ++selected;
        }
      }
    }
    return selected;
  }

  template <typename T>
  int select_historical_all(std::set<osm_edition_t> &found_eds,
                            const std::vector<osm_nwr_id_t> &ids) const {
    int selected = 0;
    for (osm_nwr_id_t id : ids) {
      using element_map_t = std::map<id_version, T>;
      id_version idv_start(id, 0);
      id_version idv_end(id+1, 0);
      const element_map_t &m = map_of<T>();
      if (!m.empty()) {
        auto itr = m.lower_bound(idv_start);
        auto end = m.lower_bound(idv_end);

        for (; itr != end; ++itr) {
          osm_edition_t ed(id, *itr->first.version);
          auto is_redacted = bool(itr->second.m_info.redaction);
          if (!is_redacted || m_redactions_visible) {
            found_eds.insert(ed);
            ++selected;
          }
        }
      }
    }
    return selected;
  }

  template <typename T>
  int select_by_changesets(
    std::set<osm_edition_t> &found_eds,
    const std::unordered_set<osm_changeset_id_t> &changesets) const {

    int selected = 0;

    for (const auto &row : map_of<T>()) {
      const T &t = row.second;
      if (changesets.count(t.m_info.changeset) > 0) {
        auto is_redacted = bool(t.m_info.redaction);
        if (!is_redacted || m_redactions_visible) {
          found_eds.emplace(t.m_info.id, t.m_info.version);
          selected += 1;
        }
      }
    }

    return selected;
  }

  database& m_db;
  std::set<osm_changeset_id_t> m_changesets;
  std::set<osm_nwr_id_t> m_nodes;
  std::set<osm_nwr_id_t> m_ways;
  std::set<osm_nwr_id_t> m_relations;
  std::set<osm_edition_t> m_historic_nodes;
  std::set<osm_edition_t> m_historic_ways;
  std::set<osm_edition_t> m_historic_relations;
  bool m_include_changeset_comments = false;
  bool m_redactions_visible = false;
  user_roles_t m_user_roles;
  oauth2_tokens m_oauth2_tokens;
};

template <>
const std::map<id_version, node> &static_data_selection::map_of<node>() const {
  return m_db.m_nodes;
}

template <>
const std::map<id_version, way> &static_data_selection::map_of<way>() const {
  return m_db.m_ways;
}

template <>
const std::map<id_version, relation> &static_data_selection::map_of<relation>() const {
  return m_db.m_relations;
}

struct factory : public data_selection::factory {
  explicit factory(const std::string &file) : factory(file, {}, {}) {}

  explicit factory(const std::string &file, const user_roles_t& user_roles, const oauth2_tokens& oauth2_tokens)
    : m_database(parse_xml(file.c_str()))
    , m_user_roles(user_roles)
    , m_oauth2_tokens(oauth2_tokens) {}

  ~factory() override = default;

  std::unique_ptr<data_selection> make_selection(Transaction_Owner_Base&) const override {
    return std::make_unique<static_data_selection>(*m_database, m_user_roles, m_oauth2_tokens);
  }

  std::unique_ptr<Transaction_Owner_Base> get_default_transaction() override {
    return std::make_unique<Transaction_Owner_Void>();
  }

private:
  std::unique_ptr<database> m_database;
  user_roles_t m_user_roles;
  oauth2_tokens m_oauth2_tokens;
};

struct staticxml_backend : public backend {
  staticxml_backend() : staticxml_backend(user_roles_t{}, oauth2_tokens{}) {}

  staticxml_backend(const user_roles_t& user_roles, const oauth2_tokens& oauth2_tokens) :
    m_user_roles(user_roles), m_oauth2_tokens(oauth2_tokens)
  {
    m_options.add_options()("file", po::value<std::string>()->required(),
                            "file to load static OSM XML from.");
  }

  ~staticxml_backend() override = default;

  [[nodiscard]] const std::string &name() const override { return m_name; }
  [[nodiscard]] const po::options_description &options() const override { return m_options; }

  std::unique_ptr<data_selection::factory> create(const po::variables_map &opts) override {
    std::string file = opts["file"].as<std::string>();
    return std::make_unique<factory>(file, m_user_roles, m_oauth2_tokens);
  }

  std::unique_ptr<data_update::factory> create_data_update(const po::variables_map &) override {
    return nullptr;   // Data update operations not supported by staticxml backend
  }

private:
  std::string m_name{"staticxml"};
  po::options_description m_options{"Static XML backend options"};
  user_roles_t m_user_roles;
  oauth2_tokens m_oauth2_tokens;
};

} // anonymous namespace


std::unique_ptr<backend> make_staticxml_backend(const user_roles_t &user_roles, const oauth2_tokens &oauth2_tokens) {
  return std::make_unique<staticxml_backend>(user_roles, oauth2_tokens);
}
