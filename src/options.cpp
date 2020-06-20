#include "cgimap/options.hpp"
#include "cgimap/logger.hpp"


global_settings_base::~global_settings_base() = default;

std::unique_ptr<global_settings_base> global_settings::settings = std::unique_ptr<global_settings_base>(new global_settings_default());


void global_settings_via_options::init_fallback_values(const global_settings_base &def) {

  m_payload_max_size = def.get_payload_max_size();
  m_map_max_nodes = def.get_map_max_nodes();
  m_map_area_max = def.get_map_area_max();
  m_changeset_timeout_open_max = def.get_changeset_timeout_open_max();
  m_changeset_timeout_idle = def.get_changeset_timeout_idle();
  m_changeset_max_elements = def.get_changeset_max_elements();
  m_way_max_nodes = def.get_way_max_nodes();
  m_scale = def.get_scale();
}

void global_settings_via_options::set_new_options(const po::variables_map &options) {

  set_payload_max_size(options);
  set_map_max_nodes(options);
  set_map_area_max(options);
  set_changeset_timeout_open_max(options);
  set_changeset_timeout_idle(options);
  set_changeset_max_elements(options);
  set_way_max_nodes(options);
  set_scale(options);
}

void global_settings_via_options::set_payload_max_size(const po::variables_map &options)  {
  if (options.count("max-payload")) {
    m_payload_max_size = options["max-payload"].as<long>();
    if (m_payload_max_size <= 0)
      throw std::invalid_argument("max-payload must be a positive number");
  }
}

void global_settings_via_options::set_map_max_nodes(const po::variables_map &options)  {
  if (options.count("map-nodes")) {
    m_map_max_nodes = options["map-nodes"].as<int>();
    if (m_map_max_nodes <= 0)
	throw std::invalid_argument("map-nodes must be a positive number");
  }
}

void global_settings_via_options::set_map_area_max(const po::variables_map &options) {
  if (options.count("map-area")) {
   m_map_area_max = options["map-area"].as<double>();
   if (m_map_area_max <= 0)
     throw std::invalid_argument("map-area must be a positive number");
  }
}

void global_settings_via_options::set_changeset_timeout_open_max(const po::variables_map &options) {
  if (options.count("changeset-timeout-open")) {
    m_changeset_timeout_open_max = options["changeset-timeout-open"].as<std::string>();
    if (!validate_timeout(m_changeset_timeout_open_max))
      throw std::invalid_argument("Invalid changeset max open timeout value");
  }
}

void global_settings_via_options::set_changeset_timeout_idle(const po::variables_map &options)  {
  if (options.count("changeset-timeout-idle")) {
    m_changeset_timeout_idle = options["changeset-timeout-idle"].as<std::string>();
    if (!validate_timeout(m_changeset_timeout_idle)) {
      throw std::invalid_argument("Invalid changeset idle timeout value");
    }
  }
}

void global_settings_via_options::set_changeset_max_elements(const po::variables_map &options)  {
  if (options.count("max-changeset-elements")) {
    m_changeset_max_elements = options["max-changeset-elements"].as<int>();
    if (m_changeset_max_elements <= 0)
      throw std::invalid_argument("max-changeset-elements must be a positive number");
  }
}

void global_settings_via_options::set_way_max_nodes(const po::variables_map &options)  {
  if (options.count("max-way-nodes")) {
    m_way_max_nodes = options["max-way-nodes"].as<int>();
    if (m_way_max_nodes <= 0)
      throw std::invalid_argument("max-way-nodes must be a positive number");
  }
}

void global_settings_via_options::set_scale(const po::variables_map &options) {
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
