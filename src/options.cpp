#include "cgimap/options.hpp"
#include "cgimap/logger.hpp"


global_settings_base::~global_settings_base() = default;

std::unique_ptr<global_settings_base> global_settings::settings = std::unique_ptr<global_settings_base>(new global_settings_default());


void global_settings_via_options::init_fallback_values(const global_settings_base & def) {

  m_payload_max_size = def.get_payload_max_size();
  m_map_max_nodes = def.get_map_max_nodes();
  m_map_area_max = def.get_map_area_max();
  m_changeset_timeout_open_max = def.get_changeset_timeout_open_max();
  m_changeset_timeout_idle = def.get_changeset_timeout_idle();
  m_changeset_max_elements = def.get_changeset_max_elements();
  m_way_max_nodes = def.get_way_max_nodes();
  m_scale = def.get_scale();
}

void global_settings_via_options::set_new_options(const po::variables_map & options) {

  set_payload_max_size(options);
  set_map_max_nodes(options);
  set_map_area_max(options);
  set_changeset_timeout_open_max(options);
  set_changeset_timeout_idle(options);
  set_changeset_max_elements(options);
  set_way_max_nodes(options);
  set_scale(options);
}

void global_settings_via_options::set_payload_max_size(const po::variables_map & options)  {
  if (options.count("max_payload_size")) {
    m_payload_max_size = options["max_payload_size"].as<long>();
    if (m_payload_max_size <= 0)
      throw std::invalid_argument("max_payload_size must be a positive number");
  }
}

void global_settings_via_options::set_map_max_nodes(const po::variables_map & options)  {
  if (options.count("map_nodes_max")) {
    m_map_max_nodes = options["map_nodes_max"].as<int>();
    if (m_map_max_nodes <= 0)
	throw std::invalid_argument("map_nodes_max must be a positive number");
  }
}

void global_settings_via_options::set_map_area_max(const po::variables_map & options) {
  if (options.count("map_area_max")) {
   m_map_area_max = options["map_area_max"].as<double>();
   if (m_map_area_max <= 0)
     throw std::invalid_argument("map_area_max must be a positive number");
  }
}

void global_settings_via_options::set_changeset_timeout_open_max(const po::variables_map & options) {
  if (options.count("changeset_timeout_open_max")) {
    m_changeset_timeout_open_max = options["changeset_timeout_open_max"].as<std::string>();
    if (!validate_timeout(m_changeset_timeout_open_max))
      throw std::invalid_argument("Invalid changeset max open timeout value");
  }
}

void global_settings_via_options::set_changeset_timeout_idle(const po::variables_map & options)  {
  if (options.count("changeset_timeout_idle")) {
    m_changeset_timeout_idle = options["changeset_timeout_idle"].as<std::string>();
    if (!validate_timeout(m_changeset_timeout_idle)) {
      throw std::invalid_argument("Invalid changeset idle timeout value");
    }
  }
}

void global_settings_via_options::set_changeset_max_elements(const po::variables_map & options)  {
  if (options.count("changeset_max_elements")) {
    m_changeset_max_elements = options["changeset_max_elements"].as<int>();
    if (m_changeset_max_elements <= 0)
      throw std::invalid_argument("changeset_max_elements must be a positive number");
  }
}

void global_settings_via_options::set_way_max_nodes(const po::variables_map & options)  {
  if (options.count("way_max_nodes")) {
    m_way_max_nodes = options["way_max_nodes"].as<int>();
    if (m_way_max_nodes <= 0)
      throw std::invalid_argument("way_max_nodes must be a positive number");
  }
}

void global_settings_via_options::set_scale(const po::variables_map & options) {
  if (options.count("scale")) {
    m_scale = options["scale"].as<long>();
    if (m_scale <= 0)
      throw std::invalid_argument("scale must be a positive number");
  }
}

bool global_settings_via_options::validate_timeout(const std::string &timeout) const {
  std::smatch sm;
  try {
    std::regex r("[0-9]+ (day|hour|minute|second)s?");
    if (std::regex_match(timeout, sm, r)) {
      return true;
    }
  } catch (std::regex_error &) {}
  return false;
}
