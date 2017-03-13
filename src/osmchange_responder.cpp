#include "cgimap/config.hpp"
#include "cgimap/osmchange_responder.hpp"

using std::list;
using boost::shared_ptr;
namespace pt = boost::posix_time;

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

bool operator<(const element &a, const element &b) {
  // length of YYYY-MM-DDTHH:MM:SSZ is 20, and strings should always be that
  // long.
  assert(a.m_info.timestamp.size() == 20);
  assert(b.m_info.timestamp.size() == 20);

  auto cmp = strncmp(
    a.m_info.timestamp.c_str(), b.m_info.timestamp.c_str(), 20);

  if (cmp < 0) {
    return true;

  } else if (cmp > 0) {
    return false;

  } else if (a.m_info.version < b.m_info.version) {
    return true;

  } else if (a.m_info.version > b.m_info.version) {
    return false;

  } else if (a.m_type < b.m_type) {
    return true;

  } else if (a.m_type > b.m_type) {
    return false;

  } else {
    return a.m_info.id < b.m_info.id;
  }
}

struct sorting_formatter
  : public output_formatter {

  virtual ~sorting_formatter() {}

  mime::type mime_type() const {
    throw std::runtime_error("sorting_formatter::mime_type unimplemented");
  }

  void start_document(
    const std::string &generator,
    const std::string &root_name) {

    throw std::runtime_error("sorting_formatter::start_document unimplemented");
  }

  void end_document() {
    throw std::runtime_error("sorting_formatter::end_document unimplemented");
  }

  void error(const std::exception &e) {
    throw std::runtime_error("sorting_formatter::error unimplemented");
  }

  void write_bounds(const bbox &bounds) {
    throw std::runtime_error("sorting_formatter::write_bounds unimplemented");
  }

  void start_element_type(element_type type) {
    throw std::runtime_error("sorting_formatter::start_element_type unimplemented");
  }

  void end_element_type(element_type type) {
    throw std::runtime_error("sorting_formatter::end_element_type unimplemented");
  }

  void write_node(
    const element_info &elem,
    double lon, double lat,
    const tags_t &tags) {

    element node{
      element_type_node, elem, tags, element::lonlat{lon, lat},
        nodes_t(), members_t()};

    m_elements.emplace_back(std::move(node));
  }

  void write_way(
    const element_info &elem,
    const nodes_t &nodes,
    const tags_t &tags) {

    element way{
      element_type_way, elem, tags, element::lonlat{},
        nodes, members_t()};

    m_elements.emplace_back(std::move(way));
  }

  void write_relation(
    const element_info &elem,
    const members_t &members,
    const tags_t &tags) {

    element rel{
      element_type_relation, elem, tags, element::lonlat{},
        nodes_t(), members};

    m_elements.emplace_back(std::move(rel));
  }

  void write_changeset(
    const changeset_info &elem,
    const tags_t &tags,
    bool include_comments,
    const comments_t &comments,
    const boost::posix_time::ptime &now) {

    throw std::runtime_error("sorting_formatter::write_changeset unimplemented");
  }

  void flush() {
    throw std::runtime_error("sorting_formatter::flush unimplemented");
  }

  // write an error to the output stream
  void error(const std::string &) {
    throw std::runtime_error("sorting_formatter::error unimplemented");
  }

  void start_action(action_type type) {
    // this shouldn't be called here, as the things which call this don't have
    // actions - they're added by this code.
    throw std::runtime_error("Unexpected call to start_action.");
  }

  void end_action(action_type type) {
    // this shouldn't be called here, as the things which call this don't have
    // actions - they're added by this code.
    throw std::runtime_error("Unexpected call to end_action.");
  }

  void write(output_formatter &fmt) {
    std::sort(m_elements.begin(), m_elements.end());
    for (const auto &e : m_elements) {
      if (e.m_info.version == 1) {
        fmt.start_action(action_type_create);
        write_element(e, fmt);
        fmt.end_action(action_type_create);

      } else if (e.m_info.visible) {
        fmt.start_action(action_type_modify);
        write_element(e, fmt);
        fmt.end_action(action_type_modify);

      } else {
        fmt.start_action(action_type_delete);
        write_element(e, fmt);
        fmt.end_action(action_type_delete);
      }
    }
  }

private:
  std::vector<element> m_elements;

  void write_element(const element &e, output_formatter &fmt) {
    switch (e.m_type) {
    case element_type_node:
      fmt.write_node(e.m_info, e.m_lonlat.m_lon, e.m_lonlat.m_lat, e.m_tags);
      break;
    case element_type_way:
      fmt.write_way(e.m_info, e.m_nds, e.m_tags);
      break;
    case element_type_relation:
      fmt.write_relation(e.m_info, e.m_members, e.m_tags);
      break;
    }
  }
};

} // anonymous namespace

osmchange_responder::osmchange_responder(
  mime::type mt, data_selection_ptr &s)
  : osm_responder(mt, boost::none), sel(s) {
}

osmchange_responder::~osmchange_responder() {
}

void osmchange_responder::write(
  shared_ptr<output_formatter> formatter,
  const std::string &generator, const pt::ptime &now) {

  // TODO: is it possible that formatter can be null?
  output_formatter &fmt = *formatter;

  fmt.start_document(generator, "osmChange");
  try {
    sorting_formatter sorter;

    sel->write_nodes(sorter);
    sel->write_ways(sorter);
    sel->write_relations(sorter);

    sorter.write(fmt);

  } catch (const std::exception &e) {
    fmt.error(e);
  }

  fmt.end_document();
}
