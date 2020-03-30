#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "parsers/saxparser.hpp"

#include <memory>

#include <boost/program_options.hpp>

/**
 * Structure for storing apidb connection information
 */
struct apidb_server_info {
  std::string host;
  std::string port;
  std::string dbname;
  std::string username;
  std::string password;
  std::string charset = "utf8";
};

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
   * Override the options file with command line arguments
   */
  void override_options(boost::program_options::variables_map &options);
  /**
   * Get configuration options as text
   */
  std::string get_configuration_options() const;
  /**
   * Get option values.
   */
  /** General Settings */
  int get_max_instances() const { return max_instances; }
  const std::string &get_pid_file_path() const { return pid_file_path; }
  const std::string &get_log_file_path() const { return log_file_path; }
  const std::string &get_socket_path() const { return socket_path; }
  const std::string &get_memcache_spec() const { return memcache_spec; }
  const int get_rate_limit() const { return rate_limit; }
  const int get_max_debt() const { return max_debt; }
  long get_input_buffer_max() const { return input_buffer_max; }
  bool get_run_as_daemon() const { return run_as_daemon; }
  /** API Backend Settings */
  const std::string &get_backend_type() const { return backend_type; }
  const std::string &get_staticxml_path() const { return staticxml_path; }
  enum BackendType {
    Default,
    Oauth,
    Update
  };
  std::string get_connect_db_str(BackendType type = BackendType::Default) const;
  const std::string& get_backend_charset(BackendType type = BackendType::Default) const
  { return get_server_info_by_type(type)->charset; }
  const std::string& get_backend_host(BackendType type = BackendType::Default) const
  { return get_server_info_by_type(type)->host; }
  const std::string& get_backend_port(BackendType type = BackendType::Default) const
  { return get_server_info_by_type(type)->port; }
  const std::string& get_backend_dbname(BackendType type = BackendType::Default) const
  { return get_server_info_by_type(type)->dbname; }
  const std::string& get_backend_username(BackendType type = BackendType::Default) const
  { return get_server_info_by_type(type)->username; }
  const std::string& get_backend_password(BackendType type = BackendType::Default) const
  { return get_server_info_by_type(type)->password; }
  const bool get_disable_api_write() const { return disable_api_write; }
  const size_t get_changeset_cache_size() const { return changeset_cache_size; }
  /** API Map Settings */
  int get_map_max_nodes() const { return map_max_nodes; }
  double get_map_area_max() const { return map_area_max; }
  /** Changeset Settings */
  const std::string &get_timeout_open_max() const { return timeout_open_max; }
  const std::string &get_timeout_idle() const { return timeout_idle; }
  /** Changeset Upload Settings */
  int get_changeset_max_elements() const { return changeset_max_elements; }
  int get_way_max_nodes() const { return way_max_nodes; }
  long get_bbox_scale() const { return bbox_scale; }
private:
  enum ParseState
  {
    General,
    Backend,
    Map,
    Changeset,
    Upload,
    None
  };
  /**
   * Overrides from SaxParser.
   */
  void on_start_element(const char *element, const char **attrs) override;
  void on_end_element(const char* element) override;
  /**
   * Run a regex on the timeout strings to validate them
   */
  bool validate_timeout(const std::string &timeout) const;
  /**
   * Override API DB info
   */
  apidb_server_info *override_apidb_server_info(
    const std::string &prefix, apidb_server_info *info,
    boost::program_options::variables_map &options);
  /**
   * Get apidb configuration options as text
   */
  std::string get_configuration_options(
    const std::string &prefix, const apidb_server_info *info) const;
  /**
   * Get the correct server info structure by the type
   */
  const apidb_server_info *get_server_info_by_type(BackendType type) const;

public:
  /** Default Values */
  static const std::string CONFIG_FILE_PATH;
  /** General Settings Default Values */
  static const int MAX_INSTANCES;
  static const std::string PID_FILE_PATH;
  static const std::string LOG_FILE_PATH;
  static const std::string SOCKET_PATH;
  static const std::string MEMCACHE_SPEC;
  static const int RATE_LIMIT;
  static const int MAX_DEBT;
  static const long BBOX_SCALE;
  static const long INPUT_BUFFER_MAX;
  static const bool RUN_AS_DAEMON;
  /** Backend Settings Default Values */
  static const std::string BACKEND_TYPE_APIDB;
  static const std::string BACKEND_TYPE_STATICXML;
  static const std::string BACKEND_TYPE;
  static const std::string STATICXML_PATH;
  static const bool DISABLE_API_WRITE;
  static const size_t CHANGESET_CACHE_SIZE;
  static const std::string DEFAULT_CHARSET;
  /** API Map Settings Default Values */
  static const int MAP_MAX_NODES;
  static const double MAP_AREA_MAX;
  /** Changeset Timeout Settings Default Values */
  static const std::string TIMEOUT_OPEN_MAX;
  static const std::string TIMEOUT_IDLE;
  /** Changeset Upload Settings Default Values */
  static const int CHANGESET_MAX_ELEMENTS;
  static const int WAY_MAX_NODES;

private:
  /** General Settings */
  int max_instances = MAX_INSTANCES;
  std::string pid_file_path = PID_FILE_PATH;
  std::string log_file_path = LOG_FILE_PATH;
  std::string socket_path = SOCKET_PATH;
  std::string memcache_spec = MEMCACHE_SPEC;
  int rate_limit = RATE_LIMIT;
  int max_debt = MAX_DEBT;
  long bbox_scale = BBOX_SCALE;
  long input_buffer_max = INPUT_BUFFER_MAX;
  bool run_as_daemon = RUN_AS_DAEMON;
  /** Backend Settings */
  std::string backend_type = BACKEND_TYPE;
  std::string staticxml_path = STATICXML_PATH;
  apidb_server_info default_server_info;
  apidb_server_info *oauth_server_info;
  apidb_server_info *update_server_info;
  bool disable_api_write = DISABLE_API_WRITE;
  size_t changeset_cache_size = CHANGESET_CACHE_SIZE;
  /** API Map Settings */
  int map_max_nodes = MAP_MAX_NODES;
  double map_area_max = MAP_AREA_MAX;
  /** Changeset Timeout Settings */
  std::string timeout_open_max = TIMEOUT_OPEN_MAX;
  std::string timeout_idle = TIMEOUT_IDLE;
  /** Changeset Upload Settings */
  int changeset_max_elements = CHANGESET_MAX_ELEMENTS;
  int way_max_nodes = WAY_MAX_NODES;
  /** Current parsing state */
  ParseState current_state = ParseState::None;
  /**
   * Default constructor/destructor.
   */
  Options();
  ~Options();
  /**
   * Remove copy constructor/assignment operator.
   */
  Options(const Options &) = delete;
  Options &operator=(const Options &) = delete;
};

#endif /* OPTIONS_HPP */
