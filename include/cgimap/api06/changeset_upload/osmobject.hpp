#ifndef OSMOBJECT_HPP
#define OSMOBJECT_HPP

#include "types.hpp"

#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <map>

class OSMObject {

public:
  OSMObject(){};

  virtual ~OSMObject() {};

  void set_changeset(osm_changeset_id_t changeset) { m_changeset = changeset; }

  void set_version(osm_version_t version) { m_version = version; }

  void set_id(osm_nwr_signed_id_t id) { m_id = id; }

  void set_visible(bool visible) { m_visible = visible; }

  // Setters with string conversions

  void set_changeset(const char *changeset) {

    set_changeset(std::stol(changeset));
  }

  void set_version(const char *version) { set_version(std::stoi(version)); }

  void set_id(const char *id) { set_id(std::stol(id)); }

  osm_changeset_id_t changeset() const { return *m_changeset; }

  osm_version_t version() const { return *m_version; }

  osm_nwr_signed_id_t id() const { return *m_id; }

  bool visible() const { return *m_visible; }

  std::map<std::string, std::string> tags() const { return m_tags; }

  void add_tag(std::string key, std::string value) {

    if (!(m_tags.insert(std::pair<std::string, std::string>(key, value)))
             .second) {
      throw http::bad_request(
          (boost::format("Element %1%/%2% has duplicate tags with key %3%") %
           get_type_name() % ((m_id) ? *m_id : 0) % key)
              .str());
    }
  }

  bool is_valid() const {
    // check if all mandatory fields have been set
    if (!m_changeset)
      throw http::bad_request(
          "You need to supply a changeset to be able to make a change");

    return (m_changeset && m_id && m_version && m_visible);
  }

  virtual std::string get_type_name() = 0;

private:
  boost::optional<osm_changeset_id_t> m_changeset;
  boost::optional<osm_nwr_signed_id_t> m_id;
  boost::optional<osm_version_t> m_version;
  boost::optional<bool> m_visible;

  std::map<std::string, std::string> m_tags;
};

#endif
