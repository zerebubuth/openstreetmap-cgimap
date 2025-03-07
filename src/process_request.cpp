/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/process_request.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/request_context.hpp"
#include "cgimap/choose_formatter.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/output_writer.hpp"
#include "cgimap/util.hpp"
#include "cgimap/oauth2.hpp"

#include <chrono>
#include <memory>
#include <tuple>

#include <fmt/core.h>


namespace {

void validate_user_db_update_permission (
    const RequestContext& req_ctx,
    data_selection& selection)
{
  if (!req_ctx.user)
    throw http::unauthorized ("User is not authorized");

  if (selection.supports_user_details())
  {
    if (selection.is_user_blocked (req_ctx.user->id))
      throw http::forbidden (
	  "Your access to the API has been blocked. Please log-in to the web interface to find out more.");

    // User status has to be either active or confirmed, otherwise the request
    // to change the database will be rejected
    if (!selection.is_user_active(req_ctx.user->id))
      throw http::forbidden (
          "You have not permitted the application access to this facility");
  }

  if (!req_ctx.user->allow_api_write)
    throw http::unauthorized ("You have not granted the modify map permission");
}

void check_db_readonly_mode (const data_update& data_update)
{
  if (data_update.is_api_write_disabled())
    throw http::bad_request (
	"Server is currently in read only mode, no database changes allowed at this time");
}


// Rails responds to ActiveRecord::RecordNotFound with an empty HTML document.
// Arguably, this isn't very useful. But it looks like we might be able to get
// more information soon:
// https://github.com/zerebubuth/openstreetmap-cgimap/pull/125#issuecomment-272720417
void respond_404(const http::not_found &e, request &r) {
  r.status(e.code())
    .add_header("Content-Type", "text/html; charset=utf-8")
    .add_header("Content-Length", "0")
    .add_header("Cache-Control", "no-cache")
    .put("");

  r.finish();
}

void respond_401(const http::unauthorized &e, request &r) {
  // According to rfc7235 we MUST send a WWW-Authenticate header field
  logger::message(fmt::format("Returning with http error {} with reason {}",
                  e.code(), e.what()));

  std::string message(e.what());

  r.status(e.code())
    .add_header("Content-Type", "text/plain; charset=utf-8")
  // Header according to RFC 7617, section 2.1
    .add_header("WWW-Authenticate", R"(Basic realm="Web Password", charset="UTF-8")")
    .add_header("Content-Length", std::to_string(message.size()))
    .add_header("Cache-Control", "no-cache")
    .put(message);      // output the message

  r.finish();
}

void response_415(const http::unsupported_media_type&e, request &r) {

  // According to rfc7694
  logger::message(fmt::format("Returning with http error {} with reason {}",
                  e.code(), e.what()));

  std::string message(e.what());

  r.status(e.code())
    .add_header("Content-Type", "text/plain; charset=utf-8")
#ifdef HAVE_LIBZ
    .add_header("Accept-Encoding", "gzip, deflate")
#else
    .add_header("Accept-Encoding", "identity")
#endif
    .add_header("Content-Length", std::to_string(message.size()))
    .add_header("Cache-Control", "no-cache")
    .put(message); // output the message

  r.finish();

}

void respond_error(const http::exception &e, request &r) {
  logger::message(fmt::format("Returning with http error {} with reason {}",
                  e.code(), e.what()));

  const char *error_format = r.get_param("HTTP_X_ERROR_FORMAT");

  if (error_format && iequals(error_format, "xml")) {
    r.status(200)
     .add_header("Content-Type", "application/xml; charset=utf-8");

    r.put(fmt::format("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n" \
        "<osmError>\r\n<status>{} {}</status>\r\n<message>{}</message>\r\n</osmError>\r\n", e.code(), e.header(), e.what()));

  } else {
    std::string message(e.what());

    std::string message_error_header = message.substr(0, 250);                           // limit HTTP header to 250 chars
    std::replace(message_error_header.begin(), message_error_header.end(), '\n', ' ');   // replace newline by space (newlines screw up HTTP header)

    r.status(e.code())
      .add_header("Content-Type", "text/plain")
      .add_header("Content-Length", std::to_string(message.size()))
      .add_header("Error", message_error_header)
      .add_header("Cache-Control", "no-cache");

    if (e.code() == 509) {
      if (auto bandwidth_exception = dynamic_cast<const http::bandwidth_limit_exceeded*>(&e)) {
        r.add_header("Retry-After",
                      std::to_string(bandwidth_exception->retry_seconds));
      }
    }

    r.put(message);   // output the message as well
  }

  r.finish();
}

/**
 * Return a 405 error.
 */

void process_not_allowed(request &req, const handler& handler) {
  req.status(405)
     .add_header("Allow", http::list_methods(handler.allowed_methods()))
     .add_header("Content-Type", "text/html")
     .add_header("Content-Length", "0")
     .add_header("Cache-Control", "no-cache")
     .finish();
}

void process_not_allowed(request &req, const http::method_not_allowed& e) {
  req.status(405)
     .add_header("Allow", http::list_methods(e.allowed_methods))
     .add_header("Content-Type", "text/html")
     .add_header("Content-Length", "0")
     .add_header("Cache-Control", "no-cache")
     .finish();
}

std::size_t generate_response(request &req, responder &responder, const std::string &generator)
{
  // get encoding to use
  auto encoding = get_encoding(req);

  // figure out best mime type
  const mime::type best_mime_type = choose_best_mime_type(req, responder);

  // TODO: use handler/responder to setup response headers.
  // write the response header
  req.status(200)
     .add_header("Content-Type", fmt::format("{}; charset=utf-8",
                                  mime::to_string(best_mime_type)))
     .add_header("Content-Encoding", encoding->name())
     .add_header("Cache-Control", "private, max-age=0, must-revalidate");

  // create the XML/JSON/text writer with the FCGI streams as output
  auto out = encoding->buffer(req.get_buffer());

  // create the correct mime type output formatter.
  auto o_formatter = create_formatter(best_mime_type, *out);

  try {
    // call to write the response
    responder.write(*o_formatter, generator, req.get_current_time());

    // ensure the request is finished
    req.finish();

    // make sure all bytes have been written. note that the writer can
    // throw an exception here, leaving the xml document in a
    // half-written state...
    o_formatter->flush();
    out->flush();

  } catch (const output_writer::write_error &e) {
    // don't do anything - just go on to the next request.
    logger::message(fmt::format("Caught write error, aborting request: {}", e.what()));

  } catch (const std::exception &e) {
    // errors here are unrecoverable (fatal to the request but maybe
    // not fatal to the process) since we already started writing to
    // the client.
    o_formatter->error(e.what());
  }

  return out->written();
}

/**
 * process a GET request.
 */
std::tuple<std::string, size_t>
process_get_request(request& req, const handler& handler,
                    data_selection& selection,
                    const std::string &ip, const std::string &generator) {
  // request start logging
  const std::string request_name = handler.log_name();
  logger::message(fmt::format("Started request for {} from {}", request_name, ip));

  // Collect all object ids (nodes/ways/relations/...) for the respective endpoint
  responder_ptr_t responder = handler.responder(selection);

  // Generate full XML/JSON/text response message for previously collected object ids
  std::size_t bytes_written = generate_response(req, *responder, generator);

  return {request_name, bytes_written};
}

/**
 * process a POST/PUT request.
 */
std::tuple<std::string, size_t>
process_post_put_request(RequestContext& req_ctx,
                         const handler& handler,
                         const data_selection::factory& factory,
                         data_update::factory& update_factory,
                         const std::string &ip,
                         const std::string &generator) {

  std::size_t bytes_written = 0;

  // request start logging
  const std::string request_name = handler.log_name();
  logger::message(fmt::format("Started request for {} from {}", request_name, ip));

  try {
    const auto & pe_handler = dynamic_cast< const payload_enabled_handler& >(handler);

    // Process request, perform database update
    {
      const auto payload = req_ctx.req.get_payload();
      auto rw_transaction = update_factory.get_default_transaction();
      auto data_update = update_factory.make_data_update(*rw_transaction);
      check_db_readonly_mode(*data_update);

      // Executing the responder constructor parses the payload, performs db CRUD operations
      // and eventually calls db commit(), in case there are no issues with the data.
      auto responder = pe_handler.responder(*data_update, payload, req_ctx);

      // does the responder instance carry all the data which is needed to construct a response?
      if (!pe_handler.requires_selection_after_update())
      {
        bytes_written = generate_response(req_ctx.req, *responder, generator);
        return {request_name, bytes_written};
      }
    }

    /*
     * Some endpoints, like changeset update, need to send back a complete OSM document
     * containing all changeset details. Since the original request payload didn't include
     * all necessary details, we need to read the data back from the database.
     *
     * As a first step, set up a new read only transaction using the update_factory,
     * then read all needed data from the db, and finally generate the response.
     */

    auto read_only_transaction = update_factory.get_read_only_transaction();

    // create a data selection for the request
    auto data_selection = factory.make_selection(*read_only_transaction);
    auto sel_responder = pe_handler.responder(*data_selection);
    bytes_written = generate_response(req_ctx.req, *sel_responder, generator);

  } catch(std::bad_cast&) {
    // This may not happen, see static_assert in routes.cpp
    throw http::server_error("HTTP method is not payload enabled");
  }

  return {request_name, bytes_written};
}


/**
 * process a HEAD request.
 */
std::tuple<std::string, size_t>
process_head_request(request& req, const handler& handler,
                     data_selection& selection,
                     const std::string &ip) {
  // request start logging
  const std::string request_name = handler.log_name();
  logger::message(fmt::format("Started HEAD request for {} from {}", request_name, ip));

  // We don't actually use the resulting data from the DB request,
  // but it might throw an error which results in a 404 or 410 response

  // The 404 and 410 responses have an empty message-body so we're safe using
  // them unmodified

  // constructor of responder handles dynamic validation (i.e: with db access).
  responder_ptr_t responder = handler.responder(selection);

  // get encoding to use
  auto encoding = get_encoding(req);

  // figure out best mime type
  const mime::type best_mime_type = choose_best_mime_type(req, *responder);

  // TODO: use handler/responder to setup response headers.
  // write the response header
  req.status(200)
     .add_header("Content-Type", fmt::format("{}; charset=utf-8",
                                  mime::to_string(best_mime_type)))
     .add_header("Content-Encoding", encoding->name())
     .add_header("Cache-Control", "no-cache");

  // ensure the request is finished
  req.finish();

  return {request_name, 0};
}

/**
 * process an OPTIONS request.
 */
std::tuple<std::string, size_t> process_options_request(
    request& req, const handler& handler) {

  static const std::string request_name = "OPTIONS";
  const char *origin = req.get_param("HTTP_ORIGIN");
  const char *method = req.get_param("HTTP_ACCESS_CONTROL_REQUEST_METHOD");

  // NOTE: we don't echo back the method - the handler already lists all
  // the methods it understands.
  if (origin && method) {

    // write the response
    req.status(200)
       .add_header("Content-Type", "text/plain");

    // if extra headers were requested, then reply that we allow them too.
    const char *headers = req.get_param("HTTP_ACCESS_CONTROL_REQUEST_HEADERS");
    if (headers) {
      req.add_header("Access-Control-Allow-Headers", std::string(headers));
    }

    // ensure the request is finished
    req.finish();

  } else {
    process_not_allowed(req, handler);
  }
  return {request_name, 0};
}

const std::string addr_prefix("addr:");
const std::string user_prefix("user:");


// look in the request get parameters to see if the user requested that
// redactions be shown
bool show_redactions_requested(const request &req) {
  std::string decoded = http::urldecode(get_query_string(req));
  const auto params = http::parse_params(decoded);
  auto itr = std::ranges::find_if(params,
    [](const auto &param) -> bool {
      return param.first == "show_redactions" && param.second == "true";
    });
  return itr != params.end();
}


// Determine user id and allow_api_write flag based on OAuth header
std::pair<std::optional<osm_user_id_t>, bool> determine_user_id(const request& req, data_selection& selection)
{
  bool allow_api_write = true;

  // Try to authenticate user via OAuth2 Bearer Token
  auto user_id = oauth2::validate_bearer_token(req, selection, allow_api_write);

  return {user_id, allow_api_write};
}

} // anonymous namespace

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter,
                     const std::string &generator, const routes &route,
                     data_selection::factory& factory,
                     data_update::factory* update_factory) {

  try {

    RequestContext req_ctx{req};

    std::setlocale(LC_ALL, "C.UTF-8");

    // get the client IP address
    const auto ip = fcgi_get_env(req, "REMOTE_ADDR");

    // fetch and parse the request method
    const auto maybe_method = http::parse_method(fcgi_get_env(req, "REQUEST_METHOD"));

    // figure how to handle the request
    auto handler = route(req);

    // if handler doesn't accept this method, then return method not
    // allowed.
    if (!maybe_method || !handler->allows_method(*maybe_method)) {
      process_not_allowed(req, *handler);
      return;
    }
    const http::method method = *maybe_method;

    // override the default access control allow methods header
    req.set_default_methods(handler->allowed_methods());

    // ------

    auto default_transaction = factory.get_default_transaction();

    // create a data selection for the request
    auto selection = factory.make_selection(*default_transaction);

    const auto [user_id, allow_api_write] = determine_user_id(req, *selection);

    // Initially assume IP based client key
    std::string client_key = addr_prefix + ip;

    // If user has been authenticated via OAuth 2, set the client key and user roles accordingly
    if (user_id) {
        client_key = (fmt::format("{}{}", user_prefix, (*user_id)));

        req_ctx.user = UserInfo{ .id = *user_id,
                                 .user_roles = selection->get_roles_for_user(*user_id),
                                 .allow_api_write = allow_api_write };
    }

    const auto is_moderator = req_ctx.is_moderator();

    // check whether the client is being rate limited
    // skip check in case of HTTP OPTIONS since it interferes with CORS preflight requests
    // see https://github.com/facebook/Rapid/issues/1424 for context
    if (method != http::method::OPTIONS) {
      if (auto [exceeded_limit, retry_seconds] = limiter.check(client_key, is_moderator);
          exceeded_limit) {
        logger::message(fmt::format("Rate limiter rejected request from {}", client_key));
        throw http::bandwidth_limit_exceeded(retry_seconds);
      }
    }

    const auto start_time = std::chrono::high_resolution_clock::now();

    if (is_moderator && show_redactions_requested(req)) {
      selection->set_redactions_visible(true);
    }

    // data returned from request methods
    std::string request_name;
    size_t bytes_written = 0;

    // process request
    switch (method) {

    case http::method::GET:
      std::tie(request_name, bytes_written) =
          process_get_request(req, *handler, *selection, ip, generator);
      break;

    case http::method::HEAD:
      std::tie(request_name, bytes_written) =
          process_head_request(req, *handler, *selection, ip);
      break;

    case http::method::POST:
    case http::method::PUT: {
      validate_user_db_update_permission(req_ctx, *selection);
      // data_selection based read only transaction no longer needed
      selection.reset(nullptr);
      default_transaction.reset(nullptr);

      if (update_factory == nullptr)
        throw http::bad_request("Backend does not support given HTTP method");

      std::tie(request_name, bytes_written) = process_post_put_request(
          req_ctx, *handler, factory, *update_factory, ip, generator);
    } break;

    case http::method::OPTIONS:
      std::tie(request_name, bytes_written) =
          process_options_request(req, *handler);
      break;

    default:
      process_not_allowed(req, *handler);
    }

    // update the rate limiter, if anything was written
    if (bytes_written > 0) {
      limiter.update(client_key, bytes_written, is_moderator);
    }

    // log the completion time (note: this comes last to avoid
    // logging twice when an error is thrown.)
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    logger::message(fmt::format("Completed request for {} from {} in {:d} ms returning {:d} bytes",
                    request_name, ip,
                    delta,
                    bytes_written));

  } catch (const http::not_found &e) {
    // most errors are passed back giving the client a choice of whether to
    // receive it as a standard HTTP error or a 200 OK with the body as an XML
    // encoded description of the error. not found errors are special - they're
    // passed back just as empty HTML documents.
    respond_404(e, req);

  } catch (const http::method_not_allowed &e) {
      process_not_allowed(req, e);

  } catch (const http::unauthorized &e) {
    // HTTP 401 unauthorized requires WWW-Authenticate header field
    respond_401(e, req);

  } catch (const http::unsupported_media_type &e) {
    // HTTP 415 unsupported media type returns list of accepted encodings
    response_415(e, req);

  } catch (const http::exception &e) {
    // errors here occur before we've started writing the response
    // so we can send something helpful back to the client.
    respond_error(e, req);

  } catch (const std::exception &e) {
    // catch an error here to provide feedback to the user
    respond_error(http::server_error(e.what()), req);

    // re-throw the exception for higher-level handling
    throw;
  }
}
