#include "cgimap/process_request.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/choose_formatter.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/output_writer.hpp"

#include <chrono>
#include <memory>
#include <sstream>
#include <tuple>

#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>


using std::runtime_error;
using std::string;
using std::ostringstream;
using std::shared_ptr;
using boost::format;

namespace al = boost::algorithm;
namespace po = boost::program_options;

namespace {

void validate_user_db_update_permission (
    const boost::optional<osm_user_id_t>& user_id,
    const std::shared_ptr<data_selection>& selection, bool allow_api_write)
{
  if (!user_id)
    throw http::unauthorized ("User is not authorized");

  if (selection->supports_user_details ()
      && selection->is_user_blocked (*user_id))
    throw http::forbidden (
	"Your access to the API has been blocked. Please log-in to the web interface to find out more.");

  if (!allow_api_write)
    throw http::unauthorized ("You have not granted the modify map permission");
}

void check_db_readonly_mode (const std::shared_ptr<data_update>& data_update)
{
  if (data_update->is_api_write_disabled())
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
}

void respond_401(const http::unauthorized &e, request &r) {
  // According to rfc7235 we MUST send a WWW-Authenticate header field
  logger::message(format("Returning with http error %1% with reason %2%") %
                  e.code() % e.what());

  std::string message(e.what());
  std::ostringstream message_size;
  message_size << message.size();

  r.status(e.code())
    .add_header("Content-Type", "text/plain; charset=utf-8")
  // Header according to RFC 7617, section 2.1
    .add_header("WWW-Authenticate", R"(Basic realm="Web Password", charset="UTF-8")")
    .add_header("Content-Length", message_size.str())
    .add_header("Cache-Control", "no-cache")
    .put(message);      // output the message

  r.finish();
}

void response_415(const http::unsupported_media_type&e, request &r) {

  // According to rfc7694
  logger::message(format("Returning with http error %1% with reason %2%") %
                  e.code() % e.what());

  std::string message(e.what());
  std::ostringstream message_size;
  message_size << message.size();

  r.status(e.code())
    .add_header("Content-Type", "text/plain; charset=utf-8")
#ifdef HAVE_LIBZ
    .add_header("Accept-Encoding", "gzip, deflate")
#else
    .add_header("Accept-Encoding", "identity")
#endif
    .add_header("Content-Length", message_size.str())
    .add_header("Cache-Control", "no-cache")
    .put(message); // output the message

  r.finish();

}

void respond_error(const http::exception &e, request &r) {
  logger::message(format("Returning with http error %1% with reason %2%") %
                  e.code() % e.what());

  const char *error_format = r.get_param("HTTP_X_ERROR_FORMAT");

  if (error_format && al::iequals(error_format, "xml")) {
    r.status(200)
     .add_header("Content-Type", "text/xml; charset=utf-8");

    ostringstream ostr;
    ostr << "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n"
         << "<osmError>\r\n"
         << "<status>" << e.code() << " " << e.header() << "</status>\r\n"
         << "<message>" << e.what() << "</message>\r\n"
         << "</osmError>\r\n";
    r.put(ostr.str());

  } else {
    std::string message(e.what());

    std::string message_error_header = message.substr(0, 250);                           // limit HTTP header to 250 chars
    std::replace(message_error_header.begin(), message_error_header.end(), '\n', ' ');   // replace newline by space (newlines screw up HTTP header)

    std::ostringstream message_size;
    message_size << message.size();

    r.status(e.code())
      .add_header("Content-Type", "text/plain")
      .add_header("Content-Length", message_size.str())
      .add_header("Error", message_error_header)
      .add_header("Cache-Control", "no-cache")
      .put(message);   // output the message as well
  }

  r.finish();
}

/**
 * Return a 405 error.
 */

void process_not_allowed(request &req, handler_ptr_t handler) {
  req.status(405)
     .add_header("Allow", http::list_methods(handler->allowed_methods()))
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

/**
 * process a GET request.
 */
std::tuple<string, size_t>
process_get_request(request &req, handler_ptr_t handler,
                    data_selection_ptr selection,
                    const string &ip, const string &generator) {
  // request start logging
  string request_name = handler->log_name();
  logger::message(format("Started request for %1% from %2%") % request_name %
                  ip);

  // constructor of responder handles dynamic validation (i.e: with db access).
  responder_ptr_t responder = handler->responder(selection);

  // get encoding to use
  shared_ptr<http::encoding> encoding = get_encoding(req);

  // figure out best mime type
  mime::type best_mime_type = choose_best_mime_type(req, responder);

  // TODO: use handler/responder to setup response headers.
  // write the response header
  req.status(200)
     .add_header("Content-Type", (boost::format("%1%; charset=utf-8") %
                                  mime::to_string(best_mime_type)).str())
     .add_header("Content-Encoding", encoding->name())
     .add_header("Cache-Control", "private, max-age=0, must-revalidate");

  // create the XML writer with the FCGI streams as output
  shared_ptr<output_buffer> out = encoding->buffer(req.get_buffer());

  // create the correct mime type output formatter.
  shared_ptr<output_formatter> o_formatter =
      create_formatter(req, best_mime_type, out);

  try {
    // call to write the response
    responder->write(o_formatter, generator, req.get_current_time());

    // ensure the request is finished
    req.finish();

    // make sure all bytes have been written. note that the writer can
    // throw an exception here, leaving the xml document in a
    // half-written state...
    o_formatter->flush();
    out->flush();

  } catch (const output_writer::write_error &e) {
    // don't do anything - just go on to the next request.
    logger::message(format("Caught write error, aborting request: %1%") %
                    e.what());

  } catch (const std::exception &e) {
    // errors here are unrecoverable (fatal to the request but maybe
    // not fatal to the process) since we already started writing to
    // the client.
    o_formatter->error(e.what());
  }

  return std::make_tuple(request_name, out->written());
}



/**
 * process a POST/PUT request.
 */
std::tuple<string, size_t>
process_post_put_request(request &req, handler_ptr_t handler,
                    std::shared_ptr<data_selection::factory> factory,
                    std::shared_ptr<data_update::factory> update_factory,
                    boost::optional<osm_user_id_t> user_id,
                    const string &ip, const string &generator) {
  // request start logging
  string request_name = handler->log_name();
  logger::message(format("Started request for %1% from %2%") % request_name %
                  ip);

  std::shared_ptr< payload_enabled_handler > pe_handler = std::static_pointer_cast< payload_enabled_handler >(handler);

  if (pe_handler == nullptr)
    throw http::server_error("HTTP method is not payload enabled");


  auto payload = req.get_payload();
  responder_ptr_t responder;

  // Step 1: execute database update
  auto rw_transaction = update_factory->get_default_transaction();

  auto data_update = update_factory->make_data_update(*rw_transaction);

  check_db_readonly_mode (data_update);

  // Executing the responder constructor is expected to call db commit()
  // rw_transaction is no longer usable after this point
  responder = pe_handler->responder(data_update, payload, user_id);

  // Step 2 (optional): read back result from database to generate response

  // Create a new read only transaction based on the update factory
  // (which possibly uses a different db/user compared to the data_selection factory)
  auto read_only_transaction = update_factory->get_read_only_transaction();
  // create a data selection for the request
  auto data_selection = factory->make_selection(*read_only_transaction);

  // Instantiate data selection based responder, if required to produce the output message
  // This instance replaces the previous responder instance, which is no longer needed.
  if (pe_handler->requires_selection_after_update()) {
      responder = pe_handler->responder(data_selection);
  }

  // get encoding to use
  shared_ptr<http::encoding> encoding = get_encoding(req);

//  // figure out best mime type
  mime::type best_mime_type = choose_best_mime_type(req, responder);

  //mime::type best_mime_type = mime::type::text_xml;

  // TODO: use handler/responder to setup response headers.
  // write the response header
  req.status(200)
     .add_header("Content-Type", (boost::format("%1%; charset=utf-8") %
                                  mime::to_string(best_mime_type)).str())
     .add_header("Content-Encoding", encoding->name())
     .add_header("Cache-Control", "private, max-age=0, must-revalidate");

  // create the XML writer with the FCGI streams as output
  shared_ptr<output_buffer> out = encoding->buffer(req.get_buffer());

  // create the correct mime type output formatter.
  shared_ptr<output_formatter> o_formatter = create_formatter(req, best_mime_type, out);

  try {
//    // call to write the response
    responder->write(o_formatter, generator, req.get_current_time());

    // ensure the request is finished
    req.finish();

    // make sure all bytes have been written. note that the writer can
    // throw an exception here, leaving the xml document in a
    // half-written state...
    o_formatter->flush();
    out->flush();

  } catch (const output_writer::write_error &e) {
    // don't do anything - just go on to the next request.
    logger::message(format("Caught write error, aborting request: %1%") %
                    e.what());

  } catch (const std::exception &e) {
    // errors here are unrecoverable (fatal to the request but maybe
    // not fatal to the process) since we already started writing to
    // the client.
    o_formatter->error(e.what());
  }

  return std::make_tuple(request_name, out->written());
}


/**
 * process a HEAD request.
 */
std::tuple<string, size_t>
process_head_request(request &req, handler_ptr_t handler,
                     data_selection_ptr selection,
                     const string &ip) {
  // request start logging
  string request_name = handler->log_name();
  logger::message(format("Started HEAD request for %1% from %2%") %
                  request_name % ip);

  // We don't actually use the resulting data from the DB request,
  // but it might throw an error which results in a 404 or 410 response

  // The 404 and 410 responses have an empty message-body so we're safe using
  // them unmodified

  // constructor of responder handles dynamic validation (i.e: with db access).
  responder_ptr_t responder = handler->responder(selection);

  // get encoding to use
  shared_ptr<http::encoding> encoding = get_encoding(req);

  // figure out best mime type
  mime::type best_mime_type = choose_best_mime_type(req, responder);

  // TODO: use handler/responder to setup response headers.
  // write the response header
  req.status(200)
     .add_header("Content-Type", (boost::format("%1%; charset=utf-8") %
                                  mime::to_string(best_mime_type)).str())
     .add_header("Content-Encoding", encoding->name())
     .add_header("Cache-Control", "no-cache");

  // ensure the request is finished
  req.finish();

  return std::make_tuple(request_name, 0);
}

/**
 * process an OPTIONS request.
 */
std::tuple<string, size_t> process_options_request(
  request &req, handler_ptr_t handler) {

  static const string request_name = "OPTIONS";
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
  return std::make_tuple(request_name, 0);
}

const std::string addr_prefix("addr:");
const std::string user_prefix("user:");

struct is_copacetic : public boost::static_visitor<bool> {
  template <typename T>
  bool operator()(const T &) const { return false; }
};

template <>
bool is_copacetic::operator()<oauth::validity::copacetic>(
  const oauth::validity::copacetic &) const {
  return true;
}

struct get_oauth_token : public boost::static_visitor<std::string> {
  template <typename T>
  std::string operator()(const T &) const {
    throw std::runtime_error("Type does not contain an OAuth token.");
  }
};

template <>
std::string get_oauth_token::operator()<oauth::validity::copacetic>(
  const oauth::validity::copacetic &c) const {
  return c.token;
}

struct oauth_status_response : public boost::static_visitor<void> {
  void operator()(const oauth::validity::copacetic &) const {}
  void operator()(const oauth::validity::not_signed &) const {}
  void operator()(const oauth::validity::bad_request &) const {
    throw http::bad_request("Bad OAuth request.");
  }
  void operator()(const oauth::validity::unauthorized &u) const {
    std::ostringstream message;
    message << "Unauthorized OAuth request, because "
            << u.reason;
    throw http::unauthorized(message.str());
  }
};

// look in the request get parameters to see if the user requested that
// redactions be shown
bool show_redactions_requested(request &req) {
  using params_t = std::vector<std::pair<std::string, std::string> >;
  std::string decoded = http::urldecode(get_query_string(req));
  const params_t params = http::parse_params(decoded);
  auto itr = std::find_if(
    params.begin(), params.end(),
    [](const params_t::value_type &param) -> bool {
      return param.first == "show_redactions" && param.second == "true";
    });
  return itr != params.end();
}


// Determine user id and allow_api_write flag based on Basic Auth or OAuth header
boost::optional<osm_user_id_t> determine_user_id (request& req,
			        std::shared_ptr<data_selection>& selection,
			        std::shared_ptr<oauth::store>& store,
			        bool& allow_api_write)
{
  // Try to authenticate user via Basic Auth
  boost::optional<osm_user_id_t>  user_id = basicauth::authenticate_user (req, selection);

  // Try to authenticate user via OAuth token
  if (!user_id && store)
    {
      oauth::validity::validity oauth_valid = oauth::is_valid_signature (
	  req, *store, *store, *store);
      if (boost::apply_visitor (is_copacetic (), oauth_valid))
	{
	  string token = boost::apply_visitor (get_oauth_token (), oauth_valid);
	  user_id = store->get_user_id_for_token (token);
	  if (!user_id)
	    {
	      // we can get here if there's a valid OAuth signature, with an
	      // authorised token matching the secret stored in the database,
	      // but that's not assigned to a user ID. perhaps this can
	      // happen due to concurrent revocation? in any case, we don't
	      // want to go any further.
	      logger::message (
		  format ("Unable to find user ID for token %1%.") % token);
	      throw http::server_error ("Unable to find user ID for token.");
	    }
	  allow_api_write = store->allow_write_api (token);
	}
      else
	{
	  boost::apply_visitor (oauth_status_response (), oauth_valid);
	  // if we got here then oauth_status_response didn't throw, which means
	  // the request must have been unsigned.
	}
    }
  return user_id;
}

} // anonymous namespace

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter,
                     const string &generator, routes &route,
                     std::shared_ptr<data_selection::factory> factory,
                     std::shared_ptr<data_update::factory> update_factory,
                     std::shared_ptr<oauth::store> store) {
  try {

    std::set<osm_user_role_t> user_roles;

    bool allow_api_write = true;

    // get the client IP address
    string ip = fcgi_get_env(req, "REMOTE_ADDR");

    // fetch and parse the request method
    boost::optional<http::method> maybe_method =  http::parse_method(fcgi_get_env(req, "REQUEST_METHOD"));

    auto default_transaction = factory->get_default_transaction();

    // create a data selection for the request
    auto selection = factory->make_selection(*default_transaction);

    boost::optional<osm_user_id_t> user_id = determine_user_id (req, selection, store, allow_api_write);

    // Initially assume IP based client key
    string client_key = addr_prefix + ip;

    // If user has been authenticated either via Basic Auth or OAuth,
    // set the client key and user roles accordingly
    if (user_id) {
        client_key = (format("%1%%2%") % user_prefix % (*user_id)).str();
        if (store)
          user_roles = store->get_roles_for_user(*user_id);
    }

    // check whether the client is being rate limited
    if (!limiter.check(client_key)) {
      logger::message(format("Rate limiter rejected request from %1%") % client_key);
      throw http::bandwidth_limit_exceeded("You have downloaded too much data. Please try again later.");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // figure how to handle the request
    handler_ptr_t handler = route(req);

    // if handler doesn't accept this method, then return method not
    // allowed.
    if (!maybe_method || !handler->allows_method(*maybe_method)) {
      process_not_allowed(req, handler);
      return;
    }
    http::method method = *maybe_method;

    // override the default access control allow methods header
    req.set_default_methods(handler->allowed_methods());

    if (show_redactions_requested(req) &&
        (user_roles.count(osm_user_role_t::moderator) > 0)) {
      selection->set_redactions_visible(true);
    }

    // data returned from request methods
    string request_name;
    size_t bytes_written = 0;

    // process request
    switch (method) {

      case http::method::GET:
	std::tie(request_name, bytes_written) = process_get_request(req, handler, selection, ip, generator);
	break;

      case http::method::HEAD:
	std::tie(request_name, bytes_written) = process_head_request(req, handler, selection, ip);
	break;

      case http::method::POST:
	{
	  validate_user_db_update_permission(user_id, selection, allow_api_write);

	  if (update_factory == nullptr)
	    throw http::bad_request("Backend does not support POST requests");

	  std::tie(request_name, bytes_written) =
	      process_post_put_request(req, handler, factory, update_factory, user_id, ip, generator);
	}
	break;

      case http::method::PUT:
	{
	  validate_user_db_update_permission(user_id, selection, allow_api_write);

	  if (update_factory == nullptr)
	    throw http::bad_request("Backend does not support PUT requests");

	  std::tie(request_name, bytes_written) =
	      process_post_put_request(req, handler, factory, update_factory, user_id, ip, generator);
	}
	break;

      case http::method::OPTIONS:
	std::tie(request_name, bytes_written) = process_options_request(req, handler);
	break;

      default:
	process_not_allowed(req, handler);
    }

    // update the rate limiter, if anything was written
    if (bytes_written > 0) {
      limiter.update(client_key, bytes_written);
    }

    // log the completion time (note: this comes last to avoid
    // logging twice when an error is thrown.)
    auto end_time = std::chrono::high_resolution_clock::now();;
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    logger::message(format("Completed request for %1% from %2% in %3% ms returning %4% bytes") %
                    request_name % ip %
		    delta %
                    bytes_written);

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
