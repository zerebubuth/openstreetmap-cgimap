/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef CHOOSE_FORMATTER_HPP
#define CHOOSE_FORMATTER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/output_buffer.hpp"
#include "cgimap/request.hpp"

#include <memory>

/**
 * Class for parsing and handling HTTP Accept headers.
 * Processes Accept headers according to RFC 2616 to determine acceptable MIME types
 * and their relative quality values (q-values).
 */
class AcceptHeader {
public:
  /**
   * Structure to represent a single element in the Accept header.
   * Each element contains type, subtype, quality value, and additional parameters.
   */
  struct AcceptElement {
    std::string raw;           ///< Raw string representation of the Accept element
    std::string type;          ///< Media type (e.g., "text")
    std::string subtype;       ///< Media subtype (e.g., "html")
    mime::type mimeType;       ///< Corresponding mime::type value
    double q = 1.0;           ///< Quality value (0.0 to 1.0)
    std::map<std::string, std::string> params;  ///< Additional parameters
  };

  /**
   * Constructs an AcceptHeader from a header string.
   * @param header The Accept header string to parse
   * @throws http::bad_request if the header cannot be parsed
   */
  explicit AcceptHeader(std::string_view header);

  /**
   * Checks if a given MIME type is acceptable according to the Accept header.
   * @param mt The MIME type to check
   * @return true if the MIME type is acceptable
   */
  [[nodiscard]] bool is_acceptable(mime::type mt) const;

  [[nodiscard]] mime::type most_acceptable_of(const std::vector<mime::type> &available) const;

private:
  std::vector<AcceptHeader::AcceptElement> acceptedTypes;  ///< Sorted list of accepted types
  std::map<mime::type, double> mapping;      ///< MIME type to q-value mapping

  /**
   * Parses an Accept header string into AcceptElements.
   * @param header The header string to parse
   * @return Vector of parsed AcceptElements
   */
  std::vector<AcceptHeader::AcceptElement> parse(std::string_view header) const;
};

/**
 * chooses and returns the best available mime type for a given request and
 * responder given the constraints in the Accept header and so forth.
 */
mime::type choose_best_mime_type(const request &req, const responder& hptr);
mime::type choose_best_mime_type(const AcceptHeader& accept_header, const responder& hptr, const std::string& path);

/**
 * creates and initialises an output formatter which matches the MIME type
 * passed in as an argument.
 */
std::unique_ptr<output_formatter>
create_formatter(mime::type best_type, output_buffer&);

#endif /* CHOOSE_FORMATTER_HPP */
