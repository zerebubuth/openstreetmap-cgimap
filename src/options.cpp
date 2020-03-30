#include "cgimap/options.hpp"
#include "cgimap/logger.hpp"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <regex>
#include <sstream>

#include <boost/lexical_cast.hpp>

/** Default Values */
const std::string Options::CONFIG_FILE_PATH = "/usr/local/etc/cgimap/cgimap.xml";
const int Options::MAX_INSTANCES = 5;
const std::string Options::PID_FILE_PATH = "/var/run/cgimap.pid";
const std::string Options::LOG_FILE_PATH = "/var/log/cgimap/cgimap.log";
const std::string Options::SOCKET_PATH = ":8000";
const std::string Options::MEMCACHE_SPEC = ""; //  Empty
const int Options::RATE_LIMIT = 100 * 1024;  //  100 KB/sec
const int Options::MAX_DEBT = 250 * 1024 * 1024; //  250 MB
const long Options::BBOX_SCALE = 10000000L;
const long Options::INPUT_BUFFER_MAX = 50000000L;
const bool Options::RUN_AS_DAEMON = false;
const std::string Options::BACKEND_TYPE_APIDB = "apidb";
const std::string Options::BACKEND_TYPE_STATICXML = "staticxml";
const std::string Options::BACKEND_TYPE = BACKEND_TYPE_APIDB;
const std::string Options::STATICXML_PATH = "";
const bool Options::DISABLE_API_WRITE = false;
const size_t Options::CHANGESET_CACHE_SIZE = 100000;
const std::string Options::DEFAULT_CHARSET = "utf8";
const int Options::MAP_MAX_NODES = 50000;
const double Options::MAP_AREA_MAX = 0.25;
const std::string Options::TIMEOUT_OPEN_MAX = "1 day";
const std::string Options::TIMEOUT_IDLE = "1 hour";
const int Options::CHANGESET_MAX_ELEMENTS = 10000;
const int Options::WAY_MAX_NODES = 2000;

Options::Options() {
  oauth_server_info = update_server_info = &default_server_info;
}

Options::~Options() {
  if (oauth_server_info != &default_server_info) {
    delete oauth_server_info;
  }
  if (update_server_info != &default_server_info) {
    delete update_server_info;
  }
}

/**
 * boost::lexical_cast doesn't support "true"/"false" strings to boolean
 */
namespace boost {
  template<>
  bool lexical_cast<bool, std::string>(const std::string& arg) {
    std::istringstream ss(arg);
    bool b = false;
    ss >> std::boolalpha >> b;
    return b;
  }
}

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

apidb_server_info *check_attributes(const char **attrs, apidb_server_info* default_info) {
  if (attrs == NULL)
    return NULL;
  apidb_server_info *result = default_info;
  if (result == NULL)
    result = new apidb_server_info();
  while (*attrs) {
    if (!std::strcmp(attrs[0], "charset") && std::strcmp(attrs[1], "") != 0)
      result->charset = attrs[1];
    else if (!std::strcmp(attrs[0], "host") && std::strcmp(attrs[1], "") != 0)
      result->host = attrs[1];
    else if (!std::strcmp(attrs[0], "port") && std::strcmp(attrs[1], "") != 0)
      result->port = attrs[1];
    else if (!std::strcmp(attrs[0], "dbname") && std::strcmp(attrs[1], "") != 0)
      result->dbname = attrs[1];
    else if (!std::strcmp(attrs[0], "username") && std::strcmp(attrs[1], "") != 0)
      result->username = attrs[1];
    else if (!std::strcmp(attrs[0], "password") && std::strcmp(attrs[1], "") != 0)
      result->password = attrs[1];
    attrs += 2;
  }
  return result;
}

void Options::parse_file(const std::string &options_path) {
  if (!options_path.empty() && std::ifstream(options_path).good())
    xmlpp::SaxParser::parse_file(options_path);
  else
    logger::message("Options file not found, using defaults.");
}

std::string Options::get_connect_db_str(BackendType type) const {
  //  Build the connection string.
  std::ostringstream ostr;
  ostr << "dbname=" << get_backend_dbname(type);
  if (!get_backend_host(type).empty())
    ostr << " host=" << get_backend_host(type);
  if (!get_backend_username(type).empty())
    ostr << " user=" << get_backend_username(type);
  if (!get_backend_password(type).empty())
    ostr << " password=" << get_backend_password(type);
  if (!get_backend_port(type).empty())
    ostr << " port=" << get_backend_port(type);
  return ostr.str();
}

void Options::on_start_element(const char *element, const char **attrs) {
  //  Check the main groupings and update the state
  if (!std::strcmp(element, "general")) {
    current_state = ParseState::General;
  } else if (!std::strcmp(element, "map")) {
    current_state = ParseState::Map;
  } else if (!std::strcmp(element, "changeset")) {
    current_state = ParseState::Changeset;
  } else if (!std::strcmp(element, "changeset_upload")) {
    current_state = ParseState::Upload;
  } else if (!std::strcmp(element, "backend")) {
    current_state = ParseState::Backend;
    //  Parse type and write enable
    backend_type = check_attributes(attrs, "type", backend_type);
    if (backend_type.compare(BACKEND_TYPE_APIDB) != 0 && backend_type.compare(BACKEND_TYPE_STATICXML) != 0)
      throw std::invalid_argument("Unknown backend type, expected '" + BACKEND_TYPE_APIDB + "' or '" + BACKEND_TYPE_STATICXML + "'.");
  } else  {
    //  General Settings
    if (current_state == ParseState::General) {
      if (!std::strcmp(element, "instances")) {
        max_instances = check_attributes(attrs, "maximum", max_instances);
        if (max_instances <= 0)
          throw std::invalid_argument("Number of instances must be strictly positive.");
      } else if (!std::strcmp(element, "pidfile")) {
        pid_file_path = check_attributes(attrs, "path", pid_file_path);
      } else if (!std::strcmp(element, "logfile")) {
        log_file_path = check_attributes(attrs, "path", log_file_path);
      } else if (!std::strcmp(element, "socket")) {
        socket_path = check_attributes(attrs, "path", socket_path);
      } else if (!std::strcmp(element, "memcache")) {
        memcache_spec = check_attributes(attrs, "spec", memcache_spec);
      } else if (!std::strcmp(element, "ratelimit")) {
        rate_limit = check_attributes(attrs, "rate", rate_limit);
        if (rate_limit <= 0)
          throw std::invalid_argument("Invalid rate limit value");
        max_debt = check_attributes(attrs, "maxdebt", max_debt);
        if (max_debt <= 0)
          throw std::invalid_argument("Invalid rate limit max debt value");
      } else if (!std::strcmp(element, "bbox")) {
        bbox_scale = check_attributes(attrs, "scale", bbox_scale);
        if (bbox_scale <= 0)
          throw std::invalid_argument("Bounding box scale must be positive.");
      } else if (!std::strcmp(element, "input_buffer")) {
        input_buffer_max = check_attributes(attrs, "maximum", input_buffer_max);
        if (input_buffer_max <= 0)
          throw std::invalid_argument("Input buffer maxmimum size must be positive.");
      } else if (!std::strcmp(element, "runas")) {
        run_as_daemon = check_attributes(attrs, "daemon", run_as_daemon);
      }
    }
    //  Backend Settings
    else if (current_state == ParseState::Backend) {
      if (!backend_type.compare("staticxml") && !std::strcmp(element, "staticxml"))  {
        staticxml_path = check_attributes(attrs, "path", staticxml_path);
      } else if (!backend_type.compare("apidb")) {
        if (!std::strcmp(element, "apidb")) {
          check_attributes(attrs, &default_server_info);
        } else if (!std::strcmp(element, "oauth")) {
          oauth_server_info = check_attributes(attrs, NULL);
        } else if (!std::strcmp(element, "update")) {
          update_server_info = check_attributes(attrs, NULL);
        } else if (!std::strcmp(element, "cachesize")) {
          changeset_cache_size = check_attributes(attrs, "maximum", changeset_cache_size);
        } else if (!std::strcmp(element, "apidb_write"))
          disable_api_write = check_attributes(attrs, "disable", disable_api_write);
      }
    }
    //  API Map Settings
    else if (current_state == ParseState::Map) {
      if (!std::strcmp(element, "nodes")) {
        map_max_nodes = check_attributes(attrs, "maximum", map_max_nodes);
        if (map_max_nodes <= 0)
          throw std::invalid_argument("Map nodes maximum must be positive.");
      } else if (!std::strcmp(element, "area")) {
        map_area_max = check_attributes(attrs, "maximum", map_area_max);
        if (map_area_max <= 0)
          throw std::invalid_argument("Maximum map area size must be positive.");
      }
    }
    //  Changeset Timeout Settings
    else if (current_state == ParseState::Changeset) {
      if (!std::strcmp(element, "timeout")) {
        timeout_open_max = check_attributes(attrs, "open", timeout_open_max);
        if (!validate_timeout(timeout_open_max))
          throw std::invalid_argument("Invalid timeout maximum open value");
        timeout_idle = check_attributes(attrs, "idle", timeout_idle);
        if (!validate_timeout(timeout_idle))
          throw std::invalid_argument("Invalid idle timeout value");
      }
    }
    //  Changeset Upload Settings
    else if (current_state == ParseState::Upload) {
      if (!std::strcmp(element, "changeset")) {
        changeset_max_elements = check_attributes(attrs, "maximum_elements", changeset_max_elements);
        if (changeset_max_elements <= 0)
          throw std::invalid_argument("Maximum changeset elements must be positive.");
      } else if (!std::strcmp(element, "waynodes")) {
        way_max_nodes = check_attributes(attrs, "maximum", way_max_nodes);
        if (way_max_nodes <= 0)
          throw std::invalid_argument("Maximum nodes in a way must be positive.");
      }
    }
  }
}

void Options::on_end_element(const char* element)
{
  //  Check the main groupings and update the state
  if (!std::strcmp(element, "general") ||
      !std::strcmp(element, "map") ||
      !std::strcmp(element, "changeset") ||
      !std::strcmp(element, "changeset_upload")) {
    current_state = ParseState::None;
  }
}

bool Options::validate_timeout(const std::string &timeout) const {
  std::smatch sm;
  try {
    std::regex r("[0-9]+ (day|hour|minute|second)s?");
    if (std::regex_match(timeout, sm, r))
      return true;
  } catch (std::regex_error &) {}
  return false;
}

void Options::override_options(boost::program_options::variables_map &options)
{
  //  General Settings
  if (options.count("instances") && options["instances"].as<int>() != MAX_INSTANCES)
    max_instances = options["instances"].as<int>();
  if (options.count("pidfile"))
    pid_file_path = options["pidfile"].as<std::string>();
  if (options.count("logfile"))
    log_file_path = options["logfile"].as<std::string>();
  if (options.count("socket") != 0)
    socket_path = options["socket"].as<std::string>();
  else if (options.count("port") != 0) {
    std::ostringstream sock_str;
    sock_str << ":" << options["port"].as<int>();
    socket_path = sock_str.str();
  }
  if (options.count("memcache"))
    memcache_spec = options["memcache"].as<std::string>();
  if (options.count("ratelimit"))
    rate_limit = options["ratelimit"].as<int>();
  if (options.count("maxdebt"))
    max_debt = options["maxdebt"].as<int>();
  if (options.count("daemon"))
    run_as_daemon = true;
  //  Backend Settings
  if (options.count("backend") && options["backend"].as<std::string>() != BACKEND_TYPE)
    backend_type = options["backend"].as<std::string>();
  if (options.count("file"))
    staticxml_path = options["file"].as<std::string>();
  override_apidb_server_info("", &default_server_info, options);
  oauth_server_info = override_apidb_server_info("oauth-", oauth_server_info, options);
  update_server_info = override_apidb_server_info("update-", update_server_info, options);
  if (options.count("disable-api-write"))
    disable_api_write = true;
  if (options.count("cachesize") && options["cachesize"].as<size_t>() != CHANGESET_CACHE_SIZE)
    changeset_cache_size = options["cachesize"].as<size_t>();
}

apidb_server_info *Options::override_apidb_server_info(
  const std::string &prefix,
  apidb_server_info *info,
  boost::program_options::variables_map &options) {
  bool found = false;
  apidb_server_info overrides;
  if (options.count(prefix + "charset")) {
    overrides.charset = options[prefix + "charset"].as<std::string>();
    //  Because the 'charset' has a default value in 'options' it shouldn't
    //  trigger an update of all default values overridden here
    found = (overrides.charset != DEFAULT_CHARSET || info != &default_server_info);
  }
  if (options.count(prefix + "host")) {
    overrides.host = options[prefix + "host"].as<std::string>();
    found = true;
  }
  if (options.count(prefix + "dbport")) {
    overrides.port = options[prefix + "dbport"].as<std::string>();
    found = true;
  }
  if (options.count(prefix + "dbname")) {
    overrides.dbname = options[prefix + "dbname"].as<std::string>();
    found = true;
  }
  if (options.count(prefix + "username")) {
    overrides.username = options[prefix + "username"].as<std::string>();
    found = true;
  }
  if (options.count(prefix + "password")) {
    overrides.password = options[prefix + "password"].as<std::string>();
    found = true;
  }
  if (found) {
    if (prefix.empty()) {
      default_server_info = overrides;
    } else if (&default_server_info == info) {
      apidb_server_info *new_info = new apidb_server_info();
      *new_info = overrides;
      return new_info;
    }
    else {
      *info = overrides;
    }
  }
  return info;
}

std::string Options::get_configuration_options() const
{
  std::ostringstream ostr;
  ostr << std::left << std::boolalpha;
  std::_Setw w = std::setw(25);
  /** General Settings */
  ostr << "--- General Settings ---" << std::endl
       << w << "Max Instances:" << max_instances << std::endl
       << w << "PID File Path:" << pid_file_path << std::endl
       << w << "Log File Path:" << log_file_path << std::endl
       << w << "Socket:" << socket_path << std::endl
       << w << "MemCache Spec:" << memcache_spec << std::endl
       << w << "Rate Limit:" << rate_limit << std::endl
       << w << "Max Debt:" << max_debt << std::endl
       << w << "Bounding Box Scale:" << bbox_scale << std::endl
       << w << "Input Buffer Max:" << input_buffer_max << std::endl;
  /** Backend Settings */
  ostr << "--- Backend Settings ---" << std::endl
       << w << "Backend Type:" << backend_type << std::endl;
  if (backend_type == "staticxml") {
    ostr << w << "Static XML Path:" << staticxml_path << std::endl;
  } else {
    ostr << w << "Disable API write:" << disable_api_write << std::endl
         << w << "Changeset Cache Size:" << changeset_cache_size << std::endl
         << get_configuration_options("", &default_server_info)
         << get_configuration_options("Oauth ", oauth_server_info)
         << get_configuration_options("Update ", update_server_info);
  }
  /** API Map Settings */
  ostr << "--- API Map Settings ---" << std::endl
       << w << "Max Map Nodes:" << map_max_nodes << std::endl
       << w << "Max Map Area:" << map_area_max << std::endl;
  /** Changeset Timeout Settings */
  ostr << "--- Changeset Timeout Settings ---" << std::endl
       << w << "Timeout Open Max:" << timeout_open_max << std::endl
       << w << "Timeout Idle:" << timeout_idle << std::endl;
  /** Changeset Upload Settings */
  ostr << "--- Changeset Upload Settings ---" << std::endl
       << w << "Changeset Max Elements:" << changeset_max_elements << std::endl
       << w << "Way Max Nodes:" << way_max_nodes << std::endl;
  return ostr.str();
}

std::string Options::get_configuration_options(
  const std::string &prefix, const apidb_server_info *info) const {
  std::ostringstream ostr;
  ostr << std::left << std::boolalpha;
  std::_Setw w = std::setw(25);
  if ((info == &default_server_info && prefix == "") ||
      info != &default_server_info) {
    ostr << w << prefix + "Charset:" << info->charset << std::endl
         << w << prefix + "Host:" << info->host << std::endl
         << w << prefix + "Port:" << info->port << std::endl
         << w << prefix + "DB Name:" << info->dbname << std::endl
         << w << prefix + "Username:" << info->username << std::endl
         << w << prefix + "Password:" << (info->password.empty() ? "*empty*" : "*******") << std::endl;
  }
  return ostr.str();
}

const apidb_server_info *Options::get_server_info_by_type(BackendType type) const {
  switch (type)
  {
  default:
  case BackendType::Default:
    return &default_server_info;
  case BackendType::Oauth:
    return oauth_server_info;
  case BackendType::Update:
    return update_server_info;
  }
}
