/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef MIME_TYPES_HPP
#define MIME_TYPES_HPP

#include <string>
#include <string_view>

/**
 * simple set of supported mime types.
 */
namespace mime {
enum class type {
  unspecified_type, // a "null" type, used to indicate no choice.
  text_plain,
  application_xml,
  application_json,
  any_type // the "*/*" type used to mean that anything is acceptable.
};

std::string to_string(type);
type parse_from(std::string_view);
}

#endif /* MIME_TYPES_HPP */
