#ifndef HANDLER_HPP
#define HANDLER_HPP

#include "cgimap/types.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/mime_types.hpp"
#include "cgimap/data_update.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/http.hpp"

#include <chrono>
#include <list>
#include <set>
#include <string>
#include <memory>


/**
 * object which is able to respond to an already-setup request.
 */
class responder {
public:
  responder(mime::type);
  virtual ~responder();
  virtual void write(std::shared_ptr<output_formatter> f,
                     const std::string &generator,
                     const std::chrono::system_clock::time_point &now) = 0;

  mime::type resource_type() const;

  virtual std::list<mime::type> types_available() const = 0;
  bool is_available(mime::type) const;

  // quick hack to get "extra" response headers.
  virtual std::string extra_response_headers() const;

private:
  mime::type mime_type;
};

using responder_ptr_t = std::shared_ptr<responder>;

/**
 * object which is able to validate and create responders from
 * requests.
 */
class handler {
public:
  handler(mime::type default_type = mime::unspecified_type,
    http::method methods = http::method::GET | http::method::HEAD | http::method::OPTIONS);
  virtual ~handler();
  virtual std::string log_name() const = 0;
  virtual responder_ptr_t responder(data_selection_ptr &) const = 0;

  void set_resource_type(mime::type);

  // returns true if the given method is allowed on this handler.
  inline bool allows_method(http::method m) const {
    return (m & m_allowed_methods) == m;
  }

  // returns the set of methods which are allowed on this handler.
  inline http::method allowed_methods() const {
    return m_allowed_methods;
  }

protected:
  mime::type mime_type;
  http::method m_allowed_methods;
};

using handler_ptr_t = std::shared_ptr<handler>;


class payload_enabled_handler : public handler {
public:
  payload_enabled_handler(mime::type default_type = mime::unspecified_type,
    http::method methods = http::method::POST | http::method::OPTIONS);

  virtual responder_ptr_t responder(data_update_ptr &, const std::string & payload, boost::optional<osm_user_id_t> user_id) const = 0;

private:
  using handler::responder;
};

#endif /* HANDLER_HPP */
