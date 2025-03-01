/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSMOBJECT_HPP
#define OSMOBJECT_HPP

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <charconv>
#include <map>
#include <optional>
#include <string_view>

#include <fmt/core.h>

namespace api06 {

  struct payload_error : public http::bad_request {

    std::string error_code;
    std::string error_string;

    explicit payload_error(const std::string &message)
      : http::bad_request(message), error_string(message) {}
  };

  class OSMObject {

  public:
    OSMObject() = default;

    virtual ~OSMObject() = default;

    void set_changeset(osm_changeset_id_t changeset) {

      if (changeset <= 0) {
          throw payload_error("Changeset must be a positive number");
      }

      m_changeset = changeset;
    }

    void set_version(int64_t version) {

      if (version < 0) {
          throw payload_error("Version may not be negative");
      }

      m_version = version;
    }

    void set_id(osm_nwr_signed_id_t id) {

      if (id == 0) {
          throw payload_error("Id must be different from 0");
      }

      m_id = id;
    }

    // Setters with string conversions

    void set_changeset(std::string_view changeset) {

      osm_changeset_id_t _changeset = 0;

      auto [_, ec] = std::from_chars(changeset.data(), changeset.data() + changeset.size(), _changeset);

      if (ec == std::errc())
        set_changeset(_changeset);
      else if (ec == std::errc::invalid_argument)
        throw payload_error("Changeset is not numeric");
      else if (ec == std::errc::result_out_of_range)
        throw payload_error("Changeset number is too large");
      else
        throw payload_error("Unexpected parsing error");
    }

    void set_version(std::string_view version) {

      int64_t _version = 0;

      auto [_, ec] = std::from_chars(version.data(), version.data() + version.size(), _version);

      if (ec == std::errc())
        set_version(_version);
      else if (ec == std::errc::invalid_argument)
        throw payload_error("Version is not numeric");
      else if (ec == std::errc::result_out_of_range)
        throw payload_error("Version value is too large");
      else
        throw payload_error("Unexpected parsing error");
    }

    void set_id(std::string_view id) {

      osm_nwr_signed_id_t _id = 0;

      auto [_, ec] = std::from_chars(id.data(), id.data() + id.size(), _id);

      if (ec == std::errc())
        set_id(_id);
      else if (ec == std::errc::invalid_argument)
        throw payload_error("Id is not numeric");
      else if (ec == std::errc::result_out_of_range)
        throw payload_error("Id number is too large");
      else
        throw payload_error("Unexpected parsing error");
    }

    osm_changeset_id_t changeset() const { return *m_changeset; }

    osm_version_t version() const { return *m_version; }

    osm_nwr_signed_id_t id() const { return *m_id; }
    osm_nwr_signed_id_t id(osm_nwr_signed_id_t d) const { return m_id.value_or(d); }

    constexpr bool has_changeset() const {  return m_changeset.has_value(); }
    constexpr bool has_id() const { return m_id.has_value(); };
    constexpr bool has_version() const { return m_version.has_value(); }

    std::map<std::string, std::string> tags() const { return m_tags; }

    void add_tags(const std::map<std::string, std::string>& tags) {
      for (const auto& [key, value] : tags) {
        add_tag(key, value);
      }
    }

    void add_tag(const std::string& key, const std::string& value) {

      if (key.empty()) {
	  throw payload_error(fmt::format("Key may not be empty in {}", to_string()));
      }

      if (unicode_strlen(key) > 255) {
	  throw payload_error(
	      fmt::format("Key has more than 255 unicode characters in {}",  to_string()));
      }

      if (unicode_strlen(value) > 255) {
	  throw payload_error(
	      fmt::format("Value has more than 255 unicode characters in {}", to_string()));
      }

      if (!(m_tags.insert({key, value}))
	  .second) {
	  throw payload_error(
	       fmt::format("{} has duplicate tags with key {}", to_string(), key));
      }
    }

    virtual bool is_valid() const {
      // check if all mandatory fields have been set
      if (!m_changeset)
	throw payload_error(
	    "You need to supply a changeset to be able to make a change");

      auto max_tags = global_settings::get_element_max_tags();

      if (max_tags && m_tags.size() > *max_tags) {
	     throw payload_error(fmt::format("OSM element exceeds limit of {} tags", *max_tags));
      }

      return (m_changeset && m_id && m_version);
    }

    virtual std::string get_type_name() const = 0;

    virtual std::string to_string() const {

      return fmt::format("{} {:d}", get_type_name(), m_id.value_or(0));
    }

    bool operator==(const OSMObject &o) const = default;

  private:
    std::optional<osm_changeset_id_t> m_changeset;
    std::optional<osm_nwr_signed_id_t> m_id;
    std::optional<osm_version_t> m_version;

    std::map<std::string, std::string> m_tags;
  };

} // namespace api06

#endif
