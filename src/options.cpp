/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/options.hpp"

#include <string_view>
#include <ranges>

global_settings_base::~global_settings_base() = default;

std::unique_ptr<global_settings_base> global_settings::settings = std::make_unique<global_settings_default>();


void global_settings_via_options::init_fallback_values(const global_settings_base &def) {

  m_payload_max_size = def.get_payload_max_size();
  m_map_max_nodes = def.get_map_max_nodes();
  m_map_area_max = def.get_map_area_max();
  m_changeset_timeout_open_max = def.get_changeset_timeout_open_max();
  m_changeset_timeout_idle = def.get_changeset_timeout_idle();
  m_changeset_max_elements = def.get_changeset_max_elements();
  m_way_max_nodes = def.get_way_max_nodes();
  m_scale = def.get_scale();
  m_relation_max_members = def.get_relation_max_members();
  m_element_max_tags = def.get_element_max_tags();
  m_ratelimiter_ratelimit = def.get_ratelimiter_ratelimit(false);
  m_moderator_ratelimiter_ratelimit = def.get_ratelimiter_ratelimit(true);
  m_ratelimiter_maxdebt = def.get_ratelimiter_maxdebt(false);
  m_moderator_ratelimiter_maxdebt = def.get_ratelimiter_maxdebt(true);
  m_ratelimiter_upload = def.get_ratelimiter_upload();
  m_bbox_size_limiter_upload = def.get_bbox_size_limiter_upload();
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
  set_relation_max_members(options);
  set_element_max_tags(options);
  set_ratelimiter_ratelimit(options);
  set_ratelimiter_maxdebt(options);
  set_ratelimiter_upload(options);
  set_bbox_size_limiter_upload(options);
}

void global_settings_via_options::set_payload_max_size(const po::variables_map &options)  {
  if (options.count("max-payload")) {
    auto payload_max_size = options["max-payload"].as<long>();
    if (payload_max_size <= 0)
      throw std::invalid_argument("max-payload must be a positive number");
    m_payload_max_size = payload_max_size;
  }
}

void global_settings_via_options::set_map_max_nodes(const po::variables_map &options)  {
  if (options.count("map-nodes")) {
    auto map_max_nodes = options["map-nodes"].as<int>();
    if (map_max_nodes <= 0)
	throw std::invalid_argument("map-nodes must be a positive number");
    m_map_max_nodes = map_max_nodes;
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
    auto changeset_max_elements = options["max-changeset-elements"].as<int>();
    if (changeset_max_elements <= 0)
      throw std::invalid_argument("max-changeset-elements must be a positive number");
    m_changeset_max_elements = changeset_max_elements;
  }
}

void global_settings_via_options::set_way_max_nodes(const po::variables_map &options)  {
  if (options.count("max-way-nodes")) {
    auto way_max_nodes = options["max-way-nodes"].as<int>();
    if (way_max_nodes <= 0)
      throw std::invalid_argument("max-way-nodes must be a positive number");
    m_way_max_nodes = way_max_nodes;
  }
}

void global_settings_via_options::set_scale(const po::variables_map &options) {
  if (options.count("scale")) {
    m_scale = options["scale"].as<long>();
    if (m_scale <= 0)
      throw std::invalid_argument("scale must be a positive number");
  }
}

void global_settings_via_options::set_relation_max_members(const po::variables_map &options) {
  if (options.count("max-relation-members")) {
    auto relation_max_members = options["max-relation-members"].as<int>();
    if (relation_max_members <= 0)
      throw std::invalid_argument("max-relation-members must be a positive number");
    m_relation_max_members = relation_max_members;
  }
}

void global_settings_via_options::set_element_max_tags(const po::variables_map &options) {
  if (options.count("max-element-tags")) {
    auto element_max_tags = options["max-element-tags"].as<int>();
    if (element_max_tags <= 0)
      throw std::invalid_argument("max-element-tags must be a positive number");
    m_element_max_tags = element_max_tags;
  }
}

void global_settings_via_options::set_ratelimiter_ratelimit(const po::variables_map &options) {
  if (options.count("ratelimit")) {
    auto parsed_bytes_per_sec = options["ratelimit"].as<long>();
    if (parsed_bytes_per_sec <= 0)
      throw std::invalid_argument("ratelimit must be greater than zero");
    if (parsed_bytes_per_sec > 1024L * 1024L * 1024L)
      throw std::invalid_argument("ratelimit must be 1GB or less");
    m_ratelimiter_ratelimit = parsed_bytes_per_sec;
  }

  if (options.count("moderator-ratelimit")) {
    auto parsed_bytes_per_sec = options["moderator-ratelimit"].as<long>();
    if (parsed_bytes_per_sec <= 0)
      throw std::invalid_argument("moderator-ratelimit must be greater than zero");
    if (parsed_bytes_per_sec > 1024L * 1024L * 1024L)
      throw std::invalid_argument("moderator-ratelimit must be 1GB or less");
    m_moderator_ratelimiter_ratelimit = parsed_bytes_per_sec;
  }
}

void global_settings_via_options::set_ratelimiter_maxdebt(const po::variables_map &options) {
 if (options.count("maxdebt")) {
    auto parsed_max_bytes = options["maxdebt"].as<long>();
    if (parsed_max_bytes <= 0)
      throw std::invalid_argument("maxdebt must be greater than zero");
    if (parsed_max_bytes > 3500)
      throw std::invalid_argument("maxdebt (in MB) must be 3500 or less");
    m_ratelimiter_maxdebt = parsed_max_bytes * 1024 * 1024;
  }

  if (options.count("moderator-maxdebt")) {
    auto parsed_max_bytes = options["moderator-maxdebt"].as<long>();
    if (parsed_max_bytes <= 0)
      throw std::invalid_argument("moderator-maxdebt must be greater than zero");
    if (parsed_max_bytes > 3500)
      throw std::invalid_argument("moderator-maxdebt (in MB) must be 3500 or less");
    m_moderator_ratelimiter_maxdebt = parsed_max_bytes * 1024 * 1024;
  }
}

void global_settings_via_options::set_ratelimiter_upload(const po::variables_map &options) {
  if (options.count("ratelimit-upload")) {
    m_ratelimiter_upload = options["ratelimit-upload"].as<bool>();
  }
}

void global_settings_via_options::set_bbox_size_limiter_upload(const po::variables_map &options) {
  if (options.count("bbox-size-limit-upload")) {
    m_bbox_size_limiter_upload = options["bbox-size-limit-upload"].as<bool>();
  }
}

/// @brief Simplified parser for Postgresql interval format
/// @param timeout The format is a number followed by a space and a unit
///               (day, days, hour, hours, minute, minutes, second, seconds).
/// @return true, if the timeout value is valid, false otherwise
bool global_settings_via_options::validate_timeout( const std::string &timeout) const {

  constexpr std::array valid_units = {
    "day", "days", "hour", "hours", "minute", "minutes", "second", "seconds"
  };

  std::vector<std::string_view> v;
  // Split input string into a number and time unit
  for (auto parts = std::ranges::views::split(timeout, ' ');
       auto &&part : parts) {
    v.emplace_back(&*part.begin(), std::ranges::distance(part));
  }

  return (v.size() == 2 &&
          std::ranges::all_of(v[0], ::isdigit) && // check if first part is a number
          std::ranges::find(valid_units, v[1]) != valid_units.end());  // check if second part is a valid unit
}
