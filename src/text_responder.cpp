/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/text_responder.hpp"

text_responder::text_responder(mime::type mt)
    : responder(mt) {}

std::vector<mime::type> text_responder::types_available() const {
  return {mime::type::text_plain};
}

void text_responder::add_response_header(const std::string &line) {
  extra_headers << line << "\r\n";
}

std::string text_responder::extra_response_headers() const {
  return extra_headers.str();
}

void text_responder::write(output_formatter& fmt,
                           const std::string &generator,
                           const std::chrono::system_clock::time_point &now)
{
  fmt.error(output_text);
}

