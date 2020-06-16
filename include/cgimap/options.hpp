#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <memory>


class global_settings_base {

public:
  virtual ~global_settings_base();

  virtual long get_payload_max_size() = 0;
  virtual int get_map_max_nodes() = 0;
  virtual double get_map_area_max() = 0;
  virtual std::string get_changeset_timeout_open_max() = 0;
  virtual std::string get_changeset_timeout_idle() = 0;
  virtual int get_changeset_max_elements() = 0;
  virtual int get_way_max_nodes() = 0;
  virtual long get_scale() = 0;
};

class global_settings_default : public global_settings_base {

public:
  long get_payload_max_size() override {
    return 50000000L;
  }

  int get_map_max_nodes() override {
    return 50000;
  }

  double get_map_area_max() override {
     return 0.25;
  }

  std::string get_changeset_timeout_open_max() override {
     return "1 day";
  }

  std::string get_changeset_timeout_idle() override {
     return "1 hour";
  }

  int get_changeset_max_elements() override {
     return 10000;
  }

  int get_way_max_nodes() override {
     return 2000;
  }

  long get_scale() override {
     return 10000000L;
  }
};

class global_settings final {

public:
  global_settings() = delete;

  static void set_configuration(std::unique_ptr<global_settings_base> && b) { settings = std::move(b); }

  // Maximum Size of HTTP body payload accepted by uploads, after decompression
  static long get_payload_max_size() { return settings->get_payload_max_size(); }

  // Maximum number of nodes returned by the /map endpoint
  static int get_map_max_nodes() { return settings->get_map_max_nodes(); }

  // Maximum permitted area for /map endpoint
  static double get_map_area_max() { return settings->get_map_area_max(); }

  // Maximum permitted open time period for a changeset
  static std::string get_changeset_timeout_open_max() { return settings->get_changeset_timeout_open_max(); }

  // Time period that a changeset will remain open after the last edit
  static std::string get_changeset_timeout_idle() { return settings->get_changeset_timeout_idle(); }

  // Maximum number of elements permitted in one changeset
  static int get_changeset_max_elements() { return settings->get_changeset_max_elements(); }

  // Maximum number of nodes permitted in a way
  static int get_way_max_nodes() { return settings->get_way_max_nodes(); }

  // Conversion factor from double lat/lon format to internal int format used for db persistence
  static long get_scale() { return settings->get_scale(); }

private:
  static std::unique_ptr<global_settings_base> settings;  // gets initialized with global_settings_default instance
};

#endif

