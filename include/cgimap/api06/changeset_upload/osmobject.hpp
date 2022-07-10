#ifndef OSMOBJECT_HPP
#define OSMOBJECT_HPP

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <fmt/core.h>
#include <map>
#include <optional>

namespace api06 {

  struct xml_error : public http::bad_request {

    std::string error_code;
    std::string error_string;

    explicit xml_error(const std::string &message)
    : http::bad_request(message), error_code(), error_string(message) {}
  };

  class OSMObject {

  public:
    OSMObject() = default;

    virtual ~OSMObject() = default;

    void set_changeset(osm_changeset_id_t changeset) { m_changeset = changeset; }

    void set_version(osm_version_t version) { m_version = version; }

    void set_id(osm_nwr_signed_id_t id) { m_id = id; }

    // Setters with string conversions

    void set_changeset(const char *changeset) {

      osm_changeset_id_t _changeset = 0;

      try {
	  _changeset = std::stol(changeset);
      } catch (std::invalid_argument& e) {
	  throw xml_error("Changeset is not numeric");
      } catch (std::out_of_range& e) {
	  throw xml_error("Changeset number is too large");
      }

      if (_changeset <= 0) {
	  throw xml_error("Changeset must be a positive number");
      }

      set_changeset(_changeset);
    }

    void set_version(const char *version) {

      int64_t _version = 0;

      try {
	  _version = std::stoi(version);
      } catch (std::invalid_argument& e) {
	  throw xml_error("Version is not numeric");
      } catch (std::out_of_range& e) {
	  throw xml_error("Version value is too large");
      }

      if (_version < 0) {
	  throw xml_error("Version may not be negative");
      }

      set_version(_version);
    }

    void set_id(const char *id) {

      osm_nwr_signed_id_t _id = 0;

      try {
	  _id = std::stol(id);
      } catch (std::invalid_argument& e) {
	  throw xml_error("Id is not numeric");
      } catch (std::out_of_range& e) {
	  throw xml_error("Id number is too large");
      }

      if (_id == 0) {
	  throw xml_error("Id must be different from 0");
      }

      set_id(_id);
    }

    osm_changeset_id_t changeset() const { return *m_changeset; }

    osm_version_t version() const { return *m_version; }

    osm_nwr_signed_id_t id() const { return *m_id; }

    bool has_changeset() const {  return ((m_changeset) ? true : false); }
    bool has_id() const { return ((m_id) ? true : false); };
    bool has_version() const { return ((m_version) ? true : false); }


    std::map<std::string, std::string> tags() const { return m_tags; }

    void add_tag(std::string key, std::string value) {

      if (key.empty()) {
	  throw xml_error(fmt::format("Key may not be empty in {}", to_string()));
      }

      if (unicode_strlen(key) > 255) {
	  throw xml_error(
	      fmt::format("Key has more than 255 unicode characters in {}",  to_string()));
      }

      if (unicode_strlen(value) > 255) {
	  throw xml_error(
	      fmt::format("Value has more than 255 unicode characters in {}", to_string()));
      }

      if (!(m_tags.insert(std::pair<std::string, std::string>(key, value)))
	  .second) {
	  throw xml_error(
	       fmt::format("{} has duplicate tags with key {}", to_string(), key));
      }
    }

    virtual bool is_valid() const {
      // check if all mandatory fields have been set
      if (!m_changeset)
	throw xml_error(
	    "You need to supply a changeset to be able to make a change");

      if ((global_settings::get_element_max_tags()) &&
	  m_tags.size() > *global_settings::get_element_max_tags()) {
	  throw xml_error(
	      fmt::format("OSM element exceeds limit of {} tags",
                 *global_settings::get_element_max_tags()));
      }

      return (m_changeset && m_id && m_version);
    }

    virtual std::string get_type_name() = 0;

    virtual std::string to_string() {

      return fmt::format("{} {:d}", get_type_name(), ((m_id) ? *m_id : 0));
    }

  private:
    std::optional<osm_changeset_id_t> m_changeset;
    std::optional<osm_nwr_signed_id_t> m_id;
    std::optional<osm_version_t> m_version;

    std::map<std::string, std::string> m_tags;
  };

} // namespace api06

#endif
