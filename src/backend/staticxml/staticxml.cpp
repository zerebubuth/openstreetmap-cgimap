#include "cgimap/backend/staticxml/staticxml.hpp"
#include "cgimap/backend.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/api06/id_version.hpp"

#include <libxml/parser.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/make_shared.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <sstream>
#include <unordered_set>

namespace po = boost::program_options;
namespace pt = boost::posix_time;
using boost::shared_ptr;
using std::string;
using api06::id_version;

#define CACHE_SIZE (1000)

namespace {

// needed to get boost::lexical_cast<bool>(string) to work.
// see:
// http://stackoverflow.com/questions/4452136/how-do-i-use-boostlexical-cast-and-stdboolalpha-i-e-boostlexical-cast-b
struct bool_alpha {
  bool data;
  bool_alpha() {}
  bool_alpha(bool data) : data(data) {}
  operator bool() const { return data; }
  friend std::ostream &operator<<(std::ostream &out, bool_alpha b) {
    out << std::boolalpha << b.data;
    return out;
  }
  friend std::istream &operator>>(std::istream &in, bool_alpha &b) {
    in >> std::boolalpha >> b.data;
    return in;
  }
};

struct node {
  element_info m_info;
  double m_lon, m_lat;
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

template <typename T>
T get_attribute(const char *name, size_t len, const xmlChar **attributes) {
  while (*attributes != NULL) {
    if (strncmp(((const char *)(*attributes++)), name, len) == 0) {
      return boost::lexical_cast<T>((const char *)(*attributes));
    }
    ++attributes;
  }
  throw std::runtime_error(
      (boost::format("Unable to find attribute %1%.") % name).str());
}

template <typename T>
boost::optional<T> opt_attribute(const char *name, size_t len,
                                 const xmlChar **attributes) {
  while (*attributes != NULL) {
    if (strncmp(((const char *)(*attributes++)), name, len) == 0) {
      return boost::lexical_cast<T>((const char *)(*attributes));
    }
    ++attributes;
  }
  return boost::none;
}

void parse_info(element_info &info, const xmlChar **attributes) {
  info.id = get_attribute<osm_nwr_id_t>("id", 3, attributes);
  info.version = get_attribute<osm_nwr_id_t>("version", 8, attributes);
  info.changeset = get_attribute<osm_changeset_id_t>("changeset", 10, attributes);
  info.timestamp = get_attribute<std::string>("timestamp", 10, attributes);
  info.uid = opt_attribute<osm_user_id_t>("uid", 4, attributes);
  info.display_name = opt_attribute<std::string>("user", 5, attributes);
  info.visible = get_attribute<bool_alpha>("visible", 8, attributes);
  info.redaction = opt_attribute<osm_redaction_id_t>("redaction", 10, attributes);
}

void parse_changeset_info(changeset_info &info, const xmlChar **attributes) {
  info.id = get_attribute<osm_changeset_id_t>("id", 3, attributes);
  info.created_at = get_attribute<std::string>("created_at", 11, attributes);
  info.closed_at = get_attribute<std::string>("closed_at", 10, attributes);
  info.uid = opt_attribute<osm_user_id_t>("uid", 4, attributes);
  info.display_name = opt_attribute<std::string>("user", 5, attributes);

  const boost::optional<double>
    min_lat = opt_attribute<double>("min_lat", 8, attributes),
    min_lon = opt_attribute<double>("min_lon", 8, attributes),
    max_lat = opt_attribute<double>("max_lat", 8, attributes),
    max_lon = opt_attribute<double>("max_lon", 8, attributes);

  if (bool(min_lat) && bool(min_lon) && bool(max_lat) &&
      bool(max_lon)) {
    info.bounding_box = bbox(*min_lat, *min_lon, *max_lat, *max_lon);
  } else {
    info.bounding_box = boost::none;
  }

  info.num_changes = get_attribute<size_t>("num_changes", 11, attributes);
  info.comments_count = 0;
}

struct xml_parser {
  xml_parser(database *db)
    : m_db(db),
      m_cur_node(NULL),
      m_cur_way(NULL),
      m_cur_rel(NULL),
      m_cur_tags(NULL),
      m_cur_changeset(NULL),
      m_in_text(false)
    {}

  static void start_element(void *ctx, const xmlChar *name,
                            const xmlChar **attributes) {
    xml_parser *parser = static_cast<xml_parser *>(ctx);

    if (strncmp((const char *)name, "node", 5) == 0) {
      node n;
      parse_info(n.m_info, attributes);
      n.m_lon = get_attribute<double>("lon", 4, attributes);
      n.m_lat = get_attribute<double>("lat", 4, attributes);
      id_version idv(n.m_info.id, n.m_info.version);
      std::pair<std::map<id_version, node>::iterator, bool> status =
          parser->m_db->m_nodes.insert(std::make_pair(idv, n));
      parser->m_cur_node = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_node->m_tags);
      parser->m_cur_way = NULL;
      parser->m_cur_rel = NULL;
      parser->m_cur_changeset = NULL;

    } else if (strncmp((const char *)name, "way", 4) == 0) {
      way w;
      parse_info(w.m_info, attributes);
      id_version idv(w.m_info.id, w.m_info.version);
      std::pair<std::map<id_version, way>::iterator, bool> status =
          parser->m_db->m_ways.insert(std::make_pair(idv, w));
      parser->m_cur_way = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_way->m_tags);
      parser->m_cur_node = NULL;
      parser->m_cur_rel = NULL;
      parser->m_cur_changeset = NULL;

    } else if (strncmp((const char *)name, "relation", 9) == 0) {
      relation r;
      parse_info(r.m_info, attributes);
      id_version idv(r.m_info.id, r.m_info.version);
      std::pair<std::map<id_version, relation>::iterator, bool> status =
          parser->m_db->m_relations.insert(std::make_pair(idv, r));
      parser->m_cur_rel = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_rel->m_tags);
      parser->m_cur_node = NULL;
      parser->m_cur_way = NULL;
      parser->m_cur_changeset = NULL;

    } else if (strncmp((const char *)name, "changeset", 10) == 0) {
      changeset c;

      parse_changeset_info(c.m_info, attributes);
      std::pair<std::map<osm_changeset_id_t, changeset>::iterator, bool> status =
        parser->m_db->m_changesets.insert(std::make_pair(c.m_info.id, c));

      parser->m_cur_changeset = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_changeset->m_tags);
      parser->m_cur_node = NULL;
      parser->m_cur_way = NULL;
      parser->m_cur_rel = NULL;

    } else if (strncmp((const char *)name, "tag", 4) == 0) {
      if (parser->m_cur_tags != NULL) {
        std::string k = get_attribute<std::string>("k", 2, attributes);
        std::string v = get_attribute<std::string>("v", 2, attributes);

        parser->m_cur_tags->push_back(std::make_pair(k, v));
      }

    } else if (strncmp((const char *)name, "nd", 3) == 0) {
      if (parser->m_cur_way != NULL) {
        parser->m_cur_way->m_nodes.push_back(
            get_attribute<osm_nwr_id_t>("ref", 4, attributes));
      }

    } else if (strncmp((const char *)name, "member", 7) == 0) {
      if (parser->m_cur_rel != NULL) {
        member_info m;
        std::string member_type =
            get_attribute<std::string>("type", 5, attributes);

        if (member_type == "node") {
          m.type = element_type_node;
        } else if (member_type == "way") {
          m.type = element_type_way;
        } else if (member_type == "relation") {
          m.type = element_type_relation;
        } else {
          throw std::runtime_error(
              (boost::format("Unknown member type `%1%'.") % member_type)
                  .str());
        }

        m.ref = get_attribute<osm_nwr_id_t>("ref", 4, attributes);
        m.role = get_attribute<std::string>("role", 5, attributes);

        parser->m_cur_rel->m_members.push_back(m);
      }
    } else if (strncmp((const char *)name, "comment", 8) == 0) {
      if (parser->m_cur_changeset != NULL) {
        parser->m_cur_changeset->m_info.comments_count += 1;

        changeset_comment_info info;
        info.author_id = get_attribute<osm_user_id_t>("uid", 4, attributes);
        info.author_display_name = get_attribute<std::string>("user", 5, attributes);
        info.created_at = get_attribute<std::string>("date", 5, attributes);
        parser->m_cur_changeset->m_comments.push_back(info);
      }
    } else if (strncmp((const char *)name, "text", 5) == 0) {
      if ((parser->m_cur_changeset != NULL) &&
          (parser->m_cur_changeset->m_comments.size() > 0)) {
        parser->m_in_text = true;
      }
    }
  }

  static void end_element(void *ctx, const xmlChar *) {
    xml_parser *parser = static_cast<xml_parser *>(ctx);
    parser->m_in_text = false;
  }

  static void characters(void *ctx, const xmlChar *str, int len) {
    xml_parser *parser = static_cast<xml_parser *>(ctx);

    if (parser->m_in_text) {
      parser->m_cur_changeset->m_comments.back().body.append((const char *)str, len);
    }
  }

  static void warning(void *, const char *, ...) {
    // TODO
  }

  static void error(void *, const char *fmt, ...) {
    char buffer[1024];
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vsnprintf(buffer, sizeof(buffer) - 1, fmt, arg_ptr);
    va_end(arg_ptr);
    throw std::runtime_error((boost::format("XML ERROR: %1%") % buffer).str());
  }

  database *m_db;
  node *m_cur_node;
  way *m_cur_way;
  relation *m_cur_rel;
  tags_t *m_cur_tags;
  changeset *m_cur_changeset;
  bool m_in_text;
};

boost::shared_ptr<database> parse_xml(const char *filename) {
  xmlSAXHandler handler;
  memset(&handler, 0, sizeof(handler));

  handler.initialized = XML_SAX2_MAGIC;
  handler.startElement = &xml_parser::start_element;
  handler.endElement = &xml_parser::end_element;
  handler.warning = &xml_parser::warning;
  handler.error = &xml_parser::error;
  handler.characters = &xml_parser::characters;

  boost::shared_ptr<database> db = boost::make_shared<database>();
  xml_parser parser(db.get());
  int status = xmlSAXUserParseFile(&handler, &parser, filename);
  if (status != 0) {
    xmlErrorPtr err = xmlGetLastError();
    throw std::runtime_error(
        (boost::format("XML ERROR: %1%.") % err->message).str());
  }

  xmlCleanupParser();

  return db;
}

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
  explicit static_data_selection(boost::shared_ptr<database> db)
    : m_db(db)
    , m_include_changeset_comments(false)
    , m_redactions_visible(false) {}
  virtual ~static_data_selection() {}

  virtual void write_nodes(output_formatter &formatter) {
    write_elements<node>(m_historic_nodes, m_nodes, formatter);
  }

  virtual void write_ways(output_formatter &formatter) {
    write_elements<way>(m_historic_ways, m_ways, formatter);
  }

  virtual void write_relations(output_formatter &formatter) {
    write_elements<relation>(m_historic_relations, m_relations, formatter);
  }

  virtual void write_changesets(output_formatter &formatter,
                                const pt::ptime &now) {
    BOOST_FOREACH(osm_changeset_id_t id, m_changesets) {
      std::map<osm_changeset_id_t, changeset>::iterator itr = m_db->m_changesets.find(id);
      if (itr != m_db->m_changesets.end()) {
        const changeset &c = itr->second;
        formatter.write_changeset(
          c.m_info, c.m_tags, m_include_changeset_comments,
          c.m_comments, now);
      }
    }
  }

  virtual visibility_t check_node_visibility(osm_nwr_id_t id) {
    return check_visibility<node>(id);
  }

  virtual visibility_t check_way_visibility(osm_nwr_id_t id) {
    return check_visibility<way>(id);
  }

  virtual visibility_t check_relation_visibility(osm_nwr_id_t id) {
    return check_visibility<relation>(id);
  }

  virtual int select_nodes(const std::vector<osm_nwr_id_t> &ids) {
    return select<node>(m_nodes, ids);
  }

  virtual int select_ways(const std::vector<osm_nwr_id_t> &ids) {
    return select<way>(m_ways, ids);
  }

  virtual int select_relations(const std::vector<osm_nwr_id_t> &ids) {
    return select<relation>(m_relations, ids);
  }

  virtual int select_nodes_from_bbox(const bbox &bounds, int max_nodes) {
    typedef std::map<id_version, node> node_map_t;
    int selected = 0;
    const node_map_t::const_iterator end = m_db->m_nodes.end();
    for (node_map_t::const_iterator itr = m_db->m_nodes.begin();
         itr != end; ++itr) {
      node_map_t::const_iterator next = itr; ++next;
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

  virtual void select_nodes_from_relations() {
    BOOST_FOREACH(osm_nwr_id_t id, m_relations) {
      boost::optional<const relation &> r = find_current<relation>(id);
      if (r) {
        BOOST_FOREACH(const member_info &m, r->m_members) {
          if (m.type == element_type_node) {
            m_nodes.insert(m.ref);
          }
        }
      }
    }
  }

  virtual void select_ways_from_nodes() {
    typedef std::map<id_version, way> way_map_t;
    const way_map_t::const_iterator end = m_db->m_ways.end();
    for (way_map_t::const_iterator itr = m_db->m_ways.begin();
         itr != end; ++itr) {
      way_map_t::const_iterator next = itr; ++next;
      const way &w = itr->second;
      if (next == end || next->second.m_info.id != w.m_info.id) {
        BOOST_FOREACH(osm_nwr_id_t node_id, w.m_nodes) {
          if (m_nodes.count(node_id) > 0) {
            m_ways.insert(w.m_info.id);
            break;
          }
        }
      }
    }
  }

  virtual void select_ways_from_relations() {
    BOOST_FOREACH(osm_nwr_id_t id, m_relations) {
      boost::optional<const relation &> r = find_current<relation>(id);
      if (r) {
        BOOST_FOREACH(const member_info &m, r->m_members) {
          if (m.type == element_type_way) {
            m_ways.insert(m.ref);
          }
        }
      }
    }
  }

  virtual void select_relations_from_ways() {
    typedef std::map<id_version, relation> relation_map_t;
    const relation_map_t::const_iterator end = m_db->m_relations.end();
    for (relation_map_t::const_iterator itr = m_db->m_relations.begin();
         itr != end; ++itr) {
      relation_map_t::const_iterator next = itr; ++next;
      const relation &r = itr->second;
      if (next == end || next->second.m_info.id != r.m_info.id) {
        BOOST_FOREACH(const member_info &m, r.m_members) {
          if ((m.type == element_type_way) && (m_ways.count(m.ref) > 0)) {
            m_relations.insert(r.m_info.id);
            break;
          }
        }
      }
    }
  }

  virtual void select_nodes_from_way_nodes() {
    BOOST_FOREACH(osm_nwr_id_t id, m_ways) {
      boost::optional<const way &> w = find_current<way>(id);
      if (w) {
        m_nodes.insert(w->m_nodes.begin(), w->m_nodes.end());
      }
    }
  }

  virtual void select_relations_from_nodes() {
    typedef std::map<id_version, relation> relation_map_t;
    const relation_map_t::const_iterator end = m_db->m_relations.end();
    for (relation_map_t::const_iterator itr = m_db->m_relations.begin();
         itr != end; ++itr) {
      relation_map_t::const_iterator next = itr; ++next;
      const relation &r = itr->second;
      BOOST_FOREACH(const member_info &m, r.m_members) {
        if ((m.type == element_type_node) && (m_nodes.count(m.ref) > 0)) {
          m_relations.insert(r.m_info.id);
          break;
        }
      }
    }
  }

  virtual void select_relations_from_relations() {
    std::set<osm_nwr_id_t> tmp_relations;
    typedef std::map<id_version, relation> relation_map_t;
    const relation_map_t::const_iterator end = m_db->m_relations.end();
    for (relation_map_t::const_iterator itr = m_db->m_relations.begin();
         itr != end; ++itr) {
      relation_map_t::const_iterator next = itr; ++next;
      const relation &r = itr->second;
      BOOST_FOREACH(const member_info &m, r.m_members) {
        if ((m.type == element_type_relation) &&
            (m_relations.count(m.ref) > 0)) {
          tmp_relations.insert(r.m_info.id);
          break;
        }
      }
    }
    m_relations.insert(tmp_relations.begin(), tmp_relations.end());
  }

  virtual void select_relations_members_of_relations() {
    BOOST_FOREACH(osm_nwr_id_t id, m_relations) {
      boost::optional<const relation &> r = find_current<relation>(id);
      if (r) {
        BOOST_FOREACH(const member_info &m, r->m_members) {
          if (m.type == element_type_relation) {
            m_relations.insert(m.ref);
          }
        }
      }
    }
  }

  virtual bool supports_historical_versions() {
    return true;
  }

  virtual int select_historical_nodes(const std::vector<osm_edition_t> &editions) {
    return select_historical<node>(m_historic_nodes, editions);
  }

  virtual int select_nodes_with_history(const std::vector<osm_nwr_id_t> &ids) {
    return select_historical_all<node>(m_historic_nodes, ids);
  }

  virtual int select_historical_ways(const std::vector<osm_edition_t> &editions) {
    return select_historical<way>(m_historic_ways, editions);
  }

  virtual int select_ways_with_history(const std::vector<osm_nwr_id_t> &ids) {
    return select_historical_all<way>(m_historic_ways, ids);
  }

  virtual int select_historical_relations(const std::vector<osm_edition_t> &editions) {
    return select_historical<relation>(m_historic_relations, editions);
  }

  virtual int select_relations_with_history(const std::vector<osm_nwr_id_t> &ids) {
    return select_historical_all<relation>(m_historic_relations, ids);
  }

  virtual void set_redactions_visible(bool visible) {
    m_redactions_visible = visible;
  }

  virtual int select_historical_by_changesets(
    const std::vector<osm_changeset_id_t> &ids) {

    std::unordered_set<osm_changeset_id_t> changesets(ids.begin(), ids.end());

    int selected = 0;
    selected += select_by_changesets<node>(m_historic_nodes, changesets);
    selected += select_by_changesets<way>(m_historic_ways, changesets);
    selected += select_by_changesets<relation>(m_historic_relations, changesets);

    return selected;
  }

  virtual bool supports_changesets() { return true; }

  virtual int select_changesets(const std::vector<osm_changeset_id_t> &ids) {
    int selected = 0;
    BOOST_FOREACH(osm_changeset_id_t id, ids) {
      std::map<osm_changeset_id_t, changeset>::iterator itr = m_db->m_changesets.find(id);
      if (itr != m_db->m_changesets.end()) {
        m_changesets.insert(id);
        ++selected;
      }
    }
    return selected;
  }

  virtual void select_changeset_discussions() {
    m_include_changeset_comments = true;
  }

private:
  template <typename T>
  const std::map<id_version, T> &map_of() const;

  template <typename T>
  boost::optional<const T&> find_current(osm_nwr_id_t id) const {
    typedef std::map<id_version, T> element_map_t;
    id_version idv(id);
    const element_map_t &m = map_of<T>();
    if (!m.empty()) {
      typename element_map_t::const_iterator itr = m.upper_bound(idv);
      if (itr != m.begin()) {
        --itr;
        if (itr->first.id == id) {
          return itr->second;
        }
      }
    }
    return boost::none;
  }

  template <typename T>
  boost::optional<const T &> find(osm_edition_t edition) const {
    typedef std::map<id_version, T> element_map_t;
    id_version idv(edition.first, edition.second);
    const element_map_t &m = map_of<T>();
    if (!m.empty()) {
      typename element_map_t::const_iterator itr = m.find(idv);
      if (itr != m.end()) {
        return itr->second;
      }
    }
    return boost::none;
  }

  template <typename T>
  void write_elements(const std::set<osm_edition_t> &historic_ids,
                      const std::set<osm_nwr_id_t> &current_ids,
                      output_formatter &formatter) const {
    std::set<osm_edition_t> editions = historic_ids;

    BOOST_FOREACH(osm_nwr_id_t id, current_ids) {
      boost::optional<const T &> maybe = find_current<T>(id);
      if (maybe) {
        const T &t = *maybe;
        editions.insert(std::make_pair(id, t.m_info.version));
      }
    }

    BOOST_FOREACH(osm_edition_t ed, editions) {
      boost::optional<const T &> maybe = find<T>(ed);
      if (maybe) {
        const T &t = *maybe;
        write_element(t, formatter);
      }
    }
  }

  template <typename T>
  visibility_t check_visibility(osm_nwr_id_t &id) {
    boost::optional<const T &> maybe = find_current<T>(id);
    if (maybe) {
      if (maybe->m_info.visible) {
        return data_selection::exists;
      } else {
        return data_selection::deleted;
      }
    } else {
      return data_selection::non_exist;
    }
  }

  template <typename T>
  int select(std::set<osm_nwr_id_t> &found_ids,
             const std::vector<osm_nwr_id_t> select_ids) const {
    int selected = 0;
    BOOST_FOREACH(osm_nwr_id_t id, select_ids) {
      boost::optional<const T &> t = find_current<T>(id);
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
    BOOST_FOREACH(osm_edition_t ed, select_eds) {
      boost::optional<const T &> t = find<T>(ed);
      if (t) {
        bool is_redacted = bool(t->m_info.redaction);
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
    BOOST_FOREACH(osm_nwr_id_t id, ids) {
      typedef std::map<id_version, T> element_map_t;
      id_version idv_start(id, 0), idv_end(id+1, 0);
      const element_map_t &m = map_of<T>();
      if (!m.empty()) {
        typename element_map_t::const_iterator itr = m.lower_bound(idv_start);
        typename element_map_t::const_iterator end = m.lower_bound(idv_end);

        for (; itr != end; ++itr) {
          osm_edition_t ed(id, *itr->first.version);
          bool is_redacted = bool(itr->second.m_info.redaction);
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
        bool is_redacted = bool(t.m_info.redaction);
        if (!is_redacted || m_redactions_visible) {
          found_eds.emplace(t.m_info.id, t.m_info.version);
          selected += 1;
        }
      }
    }

    return selected;
  }

  boost::shared_ptr<database> m_db;
  std::set<osm_changeset_id_t> m_changesets;
  std::set<osm_nwr_id_t> m_nodes, m_ways, m_relations;
  std::set<osm_edition_t> m_historic_nodes, m_historic_ways, m_historic_relations;
  bool m_include_changeset_comments, m_redactions_visible;
};

template <>
const std::map<id_version, node> &static_data_selection::map_of<node>() const {
  return m_db->m_nodes;
}

template <>
const std::map<id_version, way> &static_data_selection::map_of<way>() const {
  return m_db->m_ways;
}

template <>
const std::map<id_version, relation> &static_data_selection::map_of<relation>() const {
  return m_db->m_relations;
}

struct factory : public data_selection::factory {
  factory(const std::string &file)
    : m_database(parse_xml(file.c_str())) {}

  virtual ~factory() {}

  virtual boost::shared_ptr<data_selection> make_selection() {
    return boost::make_shared<static_data_selection>(m_database);
  }

private:
  boost::shared_ptr<database> m_database;
};

struct staticxml_backend : public backend {
  staticxml_backend()
      : m_name("staticxml"), m_options("Static XML backend options") {
    m_options.add_options()("file", po::value<string>(),
                            "file to load static OSM XML from.");
  }
  virtual ~staticxml_backend() {}

  const string &name() const { return m_name; }
  const po::options_description &options() const { return m_options; }

  shared_ptr<data_selection::factory> create(const po::variables_map &opts) {
    std::string file = opts["file"].as<std::string>();
    return boost::make_shared<factory>(file);
  }

  boost::shared_ptr<oauth::store> create_oauth_store(
    const po::variables_map &opts) {
    return boost::shared_ptr<oauth::store>();
  }

private:
  string m_name;
  po::options_description m_options;
};

} // anonymous namespace

boost::shared_ptr<backend> make_staticxml_backend() {
  return boost::make_shared<staticxml_backend>();
}
