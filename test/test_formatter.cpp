#include "cgimap/output_formatter.hpp"
#include "test_formatter.hpp"

#include <boost/foreach.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

test_formatter::node_t::node_t(const element_info &elem_, double lon_, double lat_,
                               const tags_t &tags_)
  : elem(elem_), lon(lon_), lat(lat_), tags(tags_) {}

bool test_formatter::node_t::operator==(const node_t &other) const {
#define CMP(sym) { if ((sym) != other. sym) { return false; } }
  CMP(elem.id);
  CMP(elem.version);
  CMP(elem.changeset);
  CMP(elem.timestamp);
  CMP(elem.uid);
  CMP(elem.display_name);
  CMP(elem.visible);
  CMP(lon);
  CMP(lat);
  CMP(tags.size());
#undef CMP
  return std::equal(tags.begin(), tags.end(), other.tags.begin());
}

test_formatter::way_t::way_t(const element_info &elem_, const nodes_t &nodes_,
                             const tags_t &tags_)
  : elem(elem_), nodes(nodes_), tags(tags_) {}

bool test_formatter::way_t::operator==(const way_t &other) const {
#define CMP(sym) { if ((sym) != other. sym) { return false; } }
  CMP(elem.id);
  CMP(elem.version);
  CMP(elem.changeset);
  CMP(elem.timestamp);
  CMP(elem.uid);
  CMP(elem.display_name);
  CMP(elem.visible);
  CMP(nodes.size());
  CMP(tags.size());
#undef CMP
  return std::equal(tags.begin(), tags.end(), other.tags.begin()) &&
    std::equal(nodes.begin(), nodes.end(), other.nodes.begin());
}

test_formatter::relation_t::relation_t(const element_info &elem_,
                                       const members_t &members_,
                                       const tags_t &tags_)
  : elem(elem_), members(members_), tags(tags_) {}

bool test_formatter::relation_t::operator==(const relation_t &other) const {
#define CMP(sym) { if ((sym) != other. sym) { return false; } }
  CMP(elem.id);
  CMP(elem.version);
  CMP(elem.changeset);
  CMP(elem.timestamp);
  CMP(elem.uid);
  CMP(elem.display_name);
  CMP(elem.visible);
  CMP(members.size());
  CMP(tags.size());
#undef CMP
  return std::equal(tags.begin(), tags.end(), other.tags.begin()) &&
    std::equal(members.begin(), members.end(), other.members.begin());
}

test_formatter::changeset_t::changeset_t(const changeset_info &info,
                                         const tags_t &tags,
                                         bool include_comments,
                                         const comments_t &comments,
                                         const boost::posix_time::ptime &time)
  : m_info(info)
  , m_tags(tags)
  , m_include_comments(include_comments)
  , m_comments(comments)
  , m_time(time) {
}

bool test_formatter::changeset_t::operator==(const changeset_t &other) const {
#define CMP(sym) { if ((sym) != other. sym) { return false; } }
  CMP(m_info.id);
  CMP(m_info.created_at);
  CMP(m_info.closed_at);
  CMP(m_info.uid);
  CMP(m_info.display_name);
  CMP(m_info.bounding_box);
  CMP(m_info.num_changes);
  CMP(m_info.comments_count);
  CMP(m_tags.size());
  CMP(m_include_comments);
  if (m_include_comments) {
    CMP(m_comments.size());
    if (!std::equal(m_comments.begin(), m_comments.end(), other.m_comments.begin())) {
      return false;
    }
  }
  CMP(m_time);
#undef CMP
  return std::equal(m_tags.begin(), m_tags.end(), other.m_tags.begin());
}

test_formatter::~test_formatter() {}

mime::type test_formatter::mime_type() const {
  throw std::runtime_error("Unimplemented");
}

void test_formatter::start_document(const std::string &generator) {
}

void test_formatter::end_document() {
}

void test_formatter::write_bounds(const bbox &bounds) {
}

void test_formatter::start_element_type(element_type type) {
}

void test_formatter::end_element_type(element_type type) {
}

void test_formatter::write_node(const element_info &elem, double lon, double lat,
                                const tags_t &tags) {
  m_nodes.push_back(node_t(elem, lon, lat, tags));
}
void test_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                               const tags_t &tags) {
  m_ways.push_back(way_t(elem, nodes, tags));
}

void test_formatter::write_relation(const element_info &elem,
                                    const members_t &members, const tags_t &tags) {
  m_relations.push_back(relation_t(elem, members, tags));
}

void test_formatter::write_changeset(const changeset_info &elem, const tags_t &tags,
                                     bool include_comments, const comments_t &comments,
                                     const boost::posix_time::ptime &time) {
  m_changesets.push_back(changeset_t(elem, tags, include_comments, comments, time));
}

void test_formatter::flush() {}

void test_formatter::error(const std::exception &e) {
  throw e;
}

void test_formatter::error(const std::string &str) {
  throw std::runtime_error(str);
}

std::ostream &operator<<(std::ostream &out, const element_info &elem) {
  out << "element_info("
      << "id=" << elem.id << ", "
      << "version=" << elem.version << ", "
      << "changeset=" << elem.changeset << ", "
      << "timestamp=" << elem.timestamp << ", "
      << "uid=" << elem.uid << ", "
      << "display_name=" << elem.display_name << ", "
      << "visible=" << elem.visible << ")";
  return out;
}

std::ostream &operator<<(std::ostream &out, const test_formatter::node_t &n) {
  out << "node(" << n.elem << ", "
      << "lon=" << n.lon << ", "
      << "lat=" << n.lat << ", "
      << "tags{";
  BOOST_FOREACH(const tags_t::value_type &v, n.tags) {
    out << "\"" << v.first << "\" => \"" << v.second << "\", ";
  }
  out << "})";

  return out;
}

std::ostream &operator<<(std::ostream &out, const bbox &b) {
  out << "bbox("
      << b.minlon << ", "
      << b.minlat << ", "
      << b.maxlon << ", "
      << b.maxlat << ")";
  return out;
}

std::ostream &operator<<(std::ostream &out, const test_formatter::changeset_t &c) {
  out << "changeset(changeset_info("
      << "id=" << c.m_info.id << ", "
      << "created_at=\"" << c.m_info.created_at << "\", "
      << "closed_at=\"" << c.m_info.closed_at << "\", "
      << "uid=" << c.m_info.uid << ", "
      << "display_name=\"" << c.m_info.display_name << "\", "
      << "bounding_box=" << c.m_info.bounding_box << ", "
      << "num_changes=" << c.m_info.num_changes << ", "
      << "comments_count=" << c.m_info.comments_count << "), "
      << "tags{";
  BOOST_FOREACH(const tags_t::value_type &v, c.m_tags) {
    out << "\"" << v.first << "\" => \"" << v.second << "\", ";
  }
  out << "}, "
      << "include_comments=" << c.m_include_comments << ", "
      << "comments[";
  BOOST_FOREACH(const comments_t::value_type &v, c.m_comments) {
    out << "comment(author_id=" << v.author_id << ", "
        << "body=\"" << v.body << "\", "
        << "created_at=\"" << v.created_at << "\", "
        << "author_display_name=\"" << v.author_display_name << "\"), ";
  }
  out << "], "
      << "time=" << c.m_time
      << ")";

  return out;
}

std::ostream &operator<<(std::ostream &out, const test_formatter::way_t &w) {
  out << "way(" << w.elem << ", "
      << "[";
  BOOST_FOREACH(const nodes_t::value_type &v, w.nodes) {
    out << v << ", ";
  }
  out << "], {";
  BOOST_FOREACH(const tags_t::value_type &v, w.tags) {
    out << "\"" << v.first << "\" => \"" << v.second << "\", ";
  }
  out << "})";
}

std::ostream &operator<<(std::ostream &out, const member_info &m) {
  out << "member_info(type=" << m.type << ", "
      << "ref=" << m.ref << ", "
      << "role=\"" << m.role << "\")";
  return out;
}

std::ostream &operator<<(std::ostream &out, const test_formatter::relation_t &r) {
  out << "relation(" << r.elem << ", "
      << "[";
  BOOST_FOREACH(const member_info &m, r.members) {
    out << m << ", ";
  }
  out << "], {";
  BOOST_FOREACH(const tags_t::value_type &v, r.tags) {
    out << "\"" << v.first << "\" => \"" << v.second << "\", ";
  }
  out << "})";
}
