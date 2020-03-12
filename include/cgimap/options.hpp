#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "parsers/saxparser.hpp"

#include <memory>

/**
 * Contains support for options.
 */
class Options : private xmlpp::SaxParser {

public:
  /**
   * Singleton instance.
   */
  static Options &get_instance() {
    static Options options;
    return options;
  }
  /**
   * Load options from file.
   */
  void
  parse_file(const std::string &filename = "/usr/local/etc/cgimap/cgimap.xml");
  /**
   * Get option values.
   */
  int get_map_max_nodes() const { return map_max_nodes; }
  double get_map_area_max() const { return map_area_max; }
  int get_changeset_max_elements() const { return changeset_max_elements; }
  int get_way_max_nodes() const { return way_max_nodes; }
  int get_relation_max_members() const { return relation_max_members; }
  int get_element_max_tags() const { return element_max_tags; }
  long get_bbox_scale() const { return bbox_scale; }
  const std::string &get_timeout_open_max() const { return timeout_open_max; }
  const std::string &get_timeout_idle() const { return timeout_idle; }
  long get_input_buffer_max() const { return input_buffer_max; }

private:
  /**
   * Override from SaxParser.
   */
  void on_start_element(const char *element, const char **attrs) override;
  /**
   * Run a regex on the timeout strings to validate them
   */
  bool validate_timeout(const std::string &timeout);
  /**
   * Settings collected from various hard-coded constants across the code.
   */
  int map_max_nodes = 50000;
  double map_area_max = 0.25;
  int changeset_max_elements = 10000;
  int way_max_nodes = 2000;
  int relation_max_members = 32000;
  int element_max_tags = 5000;
  long bbox_scale = 10000000L;
  std::string timeout_open_max = "1 day";
  std::string timeout_idle = "1 hour";
  long input_buffer_max = 50000000L;
  /**
   * Default constructor/destructor.
   */
  Options() = default;
  ~Options() = default;
  /**
   * Remove copy constructor/assignment operator.
   */
  Options(const Options &) = delete;
  Options &operator=(const Options &) = delete;
};

#endif /* OPTIONS_HPP */
