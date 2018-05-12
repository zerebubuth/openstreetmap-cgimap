#ifndef OSMOBJECT_HPP
#define OSMOBJECT_HPP

#include "types.hpp"
#include "util.hpp"

#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <map>

class OSMObject {

public:
  OSMObject(){};

  virtual ~OSMObject(){};

  void set_changeset(osm_changeset_id_t changeset) { m_changeset = changeset; }

  void set_version(osm_version_t version) { m_version = version; }

  void set_id(osm_nwr_signed_id_t id) { m_id = id; }

  // Setters with string conversions

  void set_changeset(const char *changeset) {

    set_changeset(std::stol(changeset));
  }

  void set_version(const char *version) { set_version(std::stoi(version)); }

  void set_id(const char *id) { set_id(std::stol(id)); }

  osm_changeset_id_t changeset() const { return *m_changeset; }

  osm_version_t version() const { return *m_version; }

  osm_nwr_signed_id_t id() const { return *m_id; }

  bool has_changeset() const {  return ((m_changeset) ? true : false); }
  bool has_id() const { return ((m_id) ? true : false); };
  bool has_version() const { return ((m_version) ? true : false); }


  std::map<std::string, std::string> tags() const { return m_tags; }

  void add_tag(std::string key, std::string value) {

    if (key.empty()) {
      throw http::bad_request(
          (boost::format("Key may not be empty in object %1%") % to_string())
              .str());
    }

    if (unicode_strlen(key) > 255) {
      throw http::bad_request(
          (boost::format(
               "Key has more than 255 unicode characters in object %1%") %
           to_string())
              .str());
    }

    if (unicode_strlen(value) > 255) {
      throw http::bad_request(
          (boost::format(
               "Value has more than 255 unicode characters in object %1%") %
           to_string())
              .str());
    }

    if (!(m_tags.insert(std::pair<std::string, std::string>(key, value)))
             .second) {
      throw http::bad_request(
          (boost::format("Element %1% has duplicate tags with key %2%") %
           to_string() % key)
              .str());
    }
  }

  bool is_valid() const {
    // check if all mandatory fields have been set
    if (!m_changeset)
      throw http::bad_request(
          "You need to supply a changeset to be able to make a change");

    return (m_changeset && m_id && m_version);
  }

  virtual std::string get_type_name() = 0;

  virtual std::string to_string() {

    return (boost::format("%1%/%2%") % get_type_name() % ((m_id) ? *m_id : 0))
        .str();
  }

private:
  boost::optional<osm_changeset_id_t> m_changeset;
  boost::optional<osm_nwr_signed_id_t> m_id;
  boost::optional<osm_version_t> m_version;

  std::map<std::string, std::string> m_tags;
};

#endif
