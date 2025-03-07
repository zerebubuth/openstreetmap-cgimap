/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef HANDLER_HPP
#define HANDLER_HPP

#include "cgimap/output_formatter.hpp"
#include "cgimap/mime_types.hpp"
#include "cgimap/data_update.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/http.hpp"

#include <chrono>
#include <vector>
#include <string>
#include <memory>


/**
 * object which is able to respond to an already-setup request.
 */
class responder {
public:
  explicit responder(mime::type);
  virtual ~responder() = default;
  virtual void write(output_formatter& f,
                     const std::string &generator,
                     const std::chrono::system_clock::time_point &now) = 0;

  mime::type resource_type() const;

  virtual std::vector<mime::type> types_available() const = 0; // TODO: don't reconstruct on every call

  bool is_available(mime::type) const;

  // quick hack to get "extra" response headers.
  virtual std::string extra_response_headers() const;

private:
  mime::type mime_type;
};

using responder_ptr_t = std::unique_ptr<responder>;

/**
 * object which is able to validate and create responders from
 * requests.
 */
class handler {
public:
  static constexpr http::method default_methods = http::method::GET | 
                                                  http::method::HEAD | 
                                                  http::method::OPTIONS;

  handler(mime::type default_type = mime::type::unspecified_type,
          http::method methods = default_methods);
  virtual ~handler() = default;

  virtual std::string log_name() const = 0;
  virtual responder_ptr_t responder(data_selection &) const = 0;

  void set_resource_type(mime::type);

  // returns true if the given method is allowed on this handler.
  constexpr bool allows_method(http::method m) const {
    return (m & m_allowed_methods) == m;
  }

  // returns the set of methods which are allowed on this handler.
  constexpr http::method allowed_methods() const {
    return m_allowed_methods;
  }

protected:
  mime::type mime_type;
  http::method m_allowed_methods;
};

using handler_ptr_t = std::unique_ptr<handler>;


class payload_enabled_handler : public handler {
public:
  payload_enabled_handler(mime::type default_type = mime::type::unspecified_type,
                          http::method methods = http::method::POST | http::method::OPTIONS);

  // Responder used to update the database
  virtual responder_ptr_t responder(data_update &, 
                                    const std::string & payload, 
                                    const RequestContext& req_ctx) const = 0;

  // Optional responder to return XML response back to caller of the API method
  responder_ptr_t responder(data_selection &) const override = 0;

  // Indicates that this payload_enabled_handler requires the optional data_selection based handler to be called
  // after the database update
  virtual bool requires_selection_after_update() const = 0;
private:
  using handler::responder;
};

#endif /* HANDLER_HPP */
