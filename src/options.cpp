#include "cgimap/options.hpp"
#include "cgimap/logger.hpp"

#include <fstream>
#include <regex>

#include <boost/lexical_cast.hpp>

template <typename T>
static T check_attributes(const char **attrs, const char *attribute_name, const T &default_value) {
  if (attrs == NULL || attribute_name == NULL)
    return default_value;
  while (*attrs) {
    if (!std::strcmp(attrs[0], attribute_name))
    {
      try {
        return boost::lexical_cast<T>(std::string(attrs[1]));
      } catch (boost::bad_lexical_cast &) {}
    }
    attrs += 2;
  }
  return default_value;
}

void Options::parse_file(const std::string &options_path) {
  if (!options_path.empty() && std::ifstream(options_path).good())
    xmlpp::SaxParser::parse_file(options_path);
  else
    logger::message("Options file not found, using defaults.");
}

void Options::on_start_element(const char *element, const char **attrs) {
  if (!std::strcmp(element, "map_nodes")) {
    map_max_nodes = check_attributes(attrs, "maximum", map_max_nodes);
    if (map_max_nodes <= 0)
      throw std::invalid_argument("Map nodes maximum must be positive.");
  } else if (!std::strcmp(element, "area")) {
    map_area_max = check_attributes(attrs, "maximum", map_area_max);
    if (map_area_max <= 0)
      throw std::invalid_argument("Maximum map area size must be positive.");
  } else if (!std::strcmp(element, "changeset")) {
    changeset_max_elements = check_attributes(attrs, "maximum_elements", changeset_max_elements);
    if (changeset_max_elements <= 0)
      throw std::invalid_argument("Maximum changeset elements must be positive.");
  } else if (!std::strcmp(element, "waynodes")) {
    way_max_nodes = check_attributes(attrs, "maximum", way_max_nodes);
    if (way_max_nodes <= 0)
      throw std::invalid_argument("Maximum nodes in a way must be positive.");
  } else if (!std::strcmp(element, "relation_members")) {
    relation_max_members = check_attributes(attrs, "maximum", relation_max_members);
    if (relation_max_members <= 0)
      throw std::invalid_argument("Maximum relation member count must be positive.");
  } else if (!std::strcmp(element, "element_tags")) {
    element_max_tags = check_attributes(attrs, "maximum", element_max_tags);
    if (element_max_tags <= 0)
      throw std::invalid_argument("Maximum tags per element must be positive.");
  } else if (!std::strcmp(element, "bbox")) {
    bbox_scale = check_attributes(attrs, "scale", bbox_scale);
    if (bbox_scale <= 0)
      throw std::invalid_argument("Bounding box scale must be positive.");
  } else if (!std::strcmp(element, "timeout")) {
    timeout_open_max = check_attributes(attrs, "open", timeout_open_max);
    if (!validate_timeout(timeout_open_max))
      throw std::invalid_argument("Invalid timeout maximum open value");
    timeout_idle = check_attributes(attrs, "idle", timeout_idle);
    if (!validate_timeout(timeout_idle))
      throw std::invalid_argument("Invalid idle timeout value");
  } else if (!std::strcmp(element, "input_buffer")) {
    input_buffer_max = check_attributes(attrs, "maximum", input_buffer_max);
    if (input_buffer_max <= 0)
      throw std::invalid_argument("Input buffer maxmimum size must be positive.");
  }
}

bool Options::validate_timeout(const std::string &timeout) {
  std::smatch sm;
  try {
    std::regex r("[0-9]+ (day|hour|minute|second)s?");
    if (std::regex_match(timeout, sm, r))
      return true;
  } catch (std::regex_error &) {}
  return false;
}
