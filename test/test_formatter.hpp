#ifndef TEST_TEST_FORMATTER
#define TEST_TEST_FORMATTER

#include "cgimap/output_formatter.hpp"

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

  struct changeset_t {
    changeset_info m_info;
    tags_t m_tags;
    bool m_include_comments;
    comments_t m_comments;
    boost::posix_time::ptime m_time;

    changeset_t(const changeset_info &info,
                const tags_t &tags,
                bool include_comments,
                const comments_t &comments,
                const boost::posix_time::ptime &time);

    inline bool operator!=(const changeset_t &other) const {
      return !operator==(other);
    }

    bool operator==(const changeset_t &other) const;
  };

  std::vector<changeset_t> m_changesets;
  std::vector<node_t> m_nodes;

  virtual ~test_formatter();
  mime::type mime_type() const;
  void start_document(const std::string &generator);
  void end_document();
  void write_bounds(const bbox &bounds);
  void start_element_type(element_type type);
  void end_element_type(element_type type);
  void write_node(const element_info &elem, double lon, double lat,
                  const tags_t &tags);
  void write_way(const element_info &elem, const nodes_t &nodes,
                 const tags_t &tags);
  void write_relation(const element_info &elem,
                      const members_t &members, const tags_t &tags);
  void write_changeset(const changeset_info &elem, const tags_t &tags,
                       bool include_comments, const comments_t &comments,
                       const boost::posix_time::ptime &time);
  void flush();

  void error(const std::exception &e);
  void error(const std::string &str);
};

std::ostream &operator<<(std::ostream &out, const test_formatter::node_t &n);

#endif /* TEST_TEST_FORMATTER */
