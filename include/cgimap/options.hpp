#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <memory>
#include <optional>
#include <regex>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

class global_settings_base {

public:
  virtual ~global_settings_base();

  virtual uint32_t get_payload_max_size() const = 0;
  virtual uint32_t get_map_max_nodes() const = 0;
  virtual double get_map_area_max() const = 0;
  virtual std::string get_changeset_timeout_open_max() const = 0;
  virtual std::string get_changeset_timeout_idle() const = 0;
  virtual uint32_t get_changeset_max_elements() const = 0;
  virtual uint32_t get_way_max_nodes() const = 0;
  virtual int64_t get_scale() const = 0;
  virtual std::optional<uint32_t> get_relation_max_members() const = 0;
  virtual std::optional<uint32_t> get_element_max_tags() const = 0;
  virtual bool get_basic_auth_support() const = 0;
  virtual bool get_oauth_10_support() const = 0;
};

class global_settings_default : public global_settings_base {

public:
  uint32_t get_payload_max_size() const override {
    return 50000000L;
  }

  uint32_t get_map_max_nodes() const override {
    return 50000;
  }

  double get_map_area_max() const override {
     return 0.25;
  }

  std::string get_changeset_timeout_open_max() const override {
     return "1 day";
  }

  std::string get_changeset_timeout_idle() const override {
     return "1 hour";
  }

  uint32_t get_changeset_max_elements() const override {
     return 10000;
  }

  uint32_t get_way_max_nodes() const override {
     return 2000;
  }

  int64_t get_scale() const override {
     return 10000000L;
  }

  std::optional<uint32_t> get_relation_max_members() const override {
     return {};  // default: unlimited
  }

  std::optional<uint32_t> get_element_max_tags() const override {
     return {};  // default: unlimited
  }

  bool get_basic_auth_support() const override {
    return true;
  }

  virtual bool get_oauth_10_support() const override {
    return true;
  }
};

class global_settings_via_options : public global_settings_base {

public:
  global_settings_via_options() = delete;

  global_settings_via_options(const po::variables_map & options) {

    init_fallback_values(global_settings_default{}); // use default values as fallback
    set_new_options(options);
  }

  global_settings_via_options(const po::variables_map & options,
			      const global_settings_base & fallback) {

    init_fallback_values(fallback);
    set_new_options(options);
  }

  uint32_t get_payload_max_size() const override {
    return m_payload_max_size;
  }

  uint32_t get_map_max_nodes() const override {
    return m_map_max_nodes;
  }

  double get_map_area_max() const override {
     return m_map_area_max;
  }

  std::string get_changeset_timeout_open_max() const override {
     return m_changeset_timeout_open_max;
  }

  std::string get_changeset_timeout_idle() const override {
     return m_changeset_timeout_idle;
  }

  uint32_t get_changeset_max_elements() const override {
     return m_changeset_max_elements;
  }

  uint32_t get_way_max_nodes() const override {
     return m_way_max_nodes;
  }

  int64_t get_scale() const override {
     return m_scale;
  }

  std::optional<uint32_t> get_relation_max_members() const override {
     return m_relation_max_members;
  }

  std::optional<uint32_t> get_element_max_tags() const override {
     return m_element_max_tags;
  }

  bool get_basic_auth_support() const override {
    return m_basic_auth_support;
  }

  virtual bool get_oauth_10_support() const override {
    return m_oauth_10_support;
  }

private:
  void init_fallback_values(const global_settings_base &def);
  void set_new_options(const po::variables_map &options);
  void set_payload_max_size(const po::variables_map &options);
  void set_map_max_nodes(const po::variables_map &options);
  void set_map_area_max(const po::variables_map &options);
  void set_changeset_timeout_open_max(const po::variables_map &options);
  void set_changeset_timeout_idle(const po::variables_map &options);
  void set_changeset_max_elements(const po::variables_map &options);
  void set_way_max_nodes(const po::variables_map &options);
  void set_scale(const po::variables_map &options);
  void set_relation_max_members(const po::variables_map &options);
  void set_element_max_tags(const po::variables_map &options);
  void set_basic_auth_support(const po::variables_map &options);
  void set_oauth_10_support(const po::variables_map &options);
  bool validate_timeout(const std::string &timeout) const;

  uint32_t m_payload_max_size;
  uint32_t  m_map_max_nodes;
  double m_map_area_max;
  std::string m_changeset_timeout_open_max;
  std::string m_changeset_timeout_idle;
  uint32_t m_changeset_max_elements;
  uint32_t m_way_max_nodes;
  int64_t m_scale;
  std::optional<uint32_t> m_relation_max_members;
  std::optional<uint32_t> m_element_max_tags;
  bool m_basic_auth_support;
  bool m_oauth_10_support;
};

class global_settings final {

public:
  global_settings() = delete;

  static void set_configuration(std::unique_ptr<global_settings_base> && b) { settings = std::move(b); }

  // Maximum Size of HTTP body payload accepted by uploads, after decompression
  static uint32_t get_payload_max_size() { return settings->get_payload_max_size(); }

  // Maximum number of nodes returned by the /map endpoint
  static uint32_t get_map_max_nodes() { return settings->get_map_max_nodes(); }

  // Maximum permitted area for /map endpoint
  static double get_map_area_max() { return settings->get_map_area_max(); }

  // Maximum permitted open time period for a changeset
  static std::string get_changeset_timeout_open_max() { return settings->get_changeset_timeout_open_max(); }

  // Time period that a changeset will remain open after the last edit
  static std::string get_changeset_timeout_idle() { return settings->get_changeset_timeout_idle(); }

  // Maximum number of elements permitted in one changeset
  static uint32_t get_changeset_max_elements() { return settings->get_changeset_max_elements(); }

  // Maximum number of nodes permitted in a way
  static uint32_t get_way_max_nodes() { return settings->get_way_max_nodes(); }

  // Conversion factor from double lat/lon format to internal int format used for db persistence
  static int64_t get_scale() { return settings->get_scale(); }

  // Maximum number of relation members for an OSM object (may be unlimited)
  static std::optional<uint32_t> get_relation_max_members() { return settings->get_relation_max_members(); }

  // Maximum number of tags for an OSM object (may be unlimited)
  static std::optional<uint32_t> get_element_max_tags() { return settings->get_element_max_tags(); }

  // Enable HTTP basic authentication support
  static bool get_basic_auth_support() { return settings->get_basic_auth_support(); }

  // Enable legacy OAuth 1.0 support
  static bool get_oauth_10_support() { return settings->get_oauth_10_support(); }


private:
  static std::unique_ptr<global_settings_base> settings;  // gets initialized with global_settings_default instance
};


#endif

