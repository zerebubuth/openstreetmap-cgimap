#include "cgimap/output_formatter.hpp"
#include "test_formatter.hpp"

#include <boost/foreach.hpp>
#include <boost/optional/optional_io.hpp>

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
}

void test_formatter::write_relation(const element_info &elem,
                                    const members_t &members, const tags_t &tags) {
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

std::ostream &operator<<(std::ostream &out, const test_formatter::node_t &n) {
  out << "node(element_info("
      << "id=" << n.elem.id << ", "
      << "version=" << n.elem.version << ", "
      << "changeset=" << n.elem.changeset << ", "
      << "timestamp=" << n.elem.timestamp << ", "
      << "uid=" << n.elem.uid << ", "
      << "display_name=" << n.elem.display_name << ", "
      << "visible=" << n.elem.visible << "), "
      << "lon=" << n.lon << ", "
      << "lat=" << n.lat << ", "
      << "{";
  BOOST_FOREACH(const tags_t::value_type &v, n.tags) {
    out << "\"" << v.first << "\" => \"" << v.second << "\", ";
  }
  out << "})";
}

