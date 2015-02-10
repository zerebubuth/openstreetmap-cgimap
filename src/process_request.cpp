#include "cgimap/process_request.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/choose_formatter.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/output_writer.hpp"

#include <sstream>

#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/tuple/tuple.hpp>

using std::runtime_error;
using std::string;
using std::ostringstream;
using boost::shared_ptr;
using boost::format;

namespace al = boost::algorithm;
namespace pt = boost::posix_time;
namespace po = boost::program_options;

namespace {

void respond_error(const http::exception &e, request &r) {
  logger::message(format("Returning with http error %1% with reason %2%") %
                  e.code() % e.what());

  const char *error_format = r.get_param("HTTP_X_ERROR_FORMAT");

  if (error_format && al::iequals(error_format, "xml")) {
    r.status(200);
    r.add_header("Content-Type", "text/xml; charset=utf-8");

    ostringstream ostr;
    ostr << "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n"
         << "<osmError>\r\n"
         << "<status>" << e.code() << " " << e.header() << "</status>\r\n"
         << "<message>" << e.what() << "</message>\r\n"
         << "</osmError>\r\n";
    r.put(ostr.str());

  } else {
    r.status(e.code());
    r.add_header("Content-Type", "text/html");
    r.add_header("Content-Length", "0");
    r.add_header("Error", e.what());
    r.add_header("Cache-Control", "no-cache");
  }

  r.finish();
}

/**
 * Return a 405 error.
 */
void process_not_allowed(request &req) {
  req.status(405);
  req.add_header("Allow", "GET, HEAD, OPTIONS");
  req.add_header("Content-Type", "text/html");
  req.add_header("Content-Length", "0");
  req.add_header("Cache-Control", "no-cache");
  req.finish();
}

/**
 * process a GET request.
 */
boost::tuple<string, size_t>
process_get_request(request &req, routes &route,
                    boost::shared_ptr<data_selection::factory> factory,
                    const string &ip, const string &generator) {
  // figure how to handle the request
  handler_ptr_t handler = route(req);

  // request start logging
  string request_name = handler->log_name();
  logger::message(format("Started request for %1% from %2%") % request_name %
                  ip);

  // constructor of responder handles dynamic validation (i.e: with db access).
  responder_ptr_t responder = handler->responder(factory);

  // get encoding to use
  shared_ptr<http::encoding> encoding = get_encoding(req);

  // figure out best mime type
  mime::type best_mime_type = choose_best_mime_type(req, responder);

  // TODO: use handler/responder to setup response headers.
  // write the response header
  req.status(200);
  req.add_header("Content-Type", (boost::format("%1%; charset=utf-8") %
                                  mime::to_string(best_mime_type)).str());
  req.add_header("Content-Encoding", encoding->name());
  req.add_header("Cache-Control", "private, max-age=0, must-revalidate");

  // create the XML writer with the FCGI streams as output
  shared_ptr<output_buffer> out = encoding->buffer(req.get_buffer());

  // create the correct mime type output formatter.
  shared_ptr<output_formatter> o_formatter =
      create_formatter(req, best_mime_type, out);

  try {
    // call to write the response
    responder->write(o_formatter, generator);

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

  return boost::make_tuple(request_name, out->written());
}

/**
 * process a HEAD request.
 */
boost::tuple<string, size_t>
process_head_request(request &req, routes &route,
                     boost::shared_ptr<data_selection::factory> factory,
                     const string &ip) {
  // figure how to handle the request
  handler_ptr_t handler = route(req);

  // request start logging
  string request_name = handler->log_name();
  logger::message(format("Started HEAD request for %1% from %2%") %
                  request_name % ip);

  // We don't actually use the resulting data from the DB request,
  // but it might throw an error which results in a 404 or 410 response

  // The 404 and 410 responses have an empty message-body so we're safe using
  // them unmodified

  // constructor of responder handles dynamic validation (i.e: with db access).
  responder_ptr_t responder = handler->responder(factory);

  // get encoding to use
  shared_ptr<http::encoding> encoding = get_encoding(req);

  // figure out best mime type
  mime::type best_mime_type = choose_best_mime_type(req, responder);

  // TODO: use handler/responder to setup response headers.
  // write the response header
  req.status(200);
  req.add_header("Content-Type", (boost::format("%1%; charset=utf-8") %
                                  mime::to_string(best_mime_type)).str());
  req.add_header("Content-Encoding", encoding->name());
  req.add_header("Cache-Control", "no-cache");

  // ensure the request is finished
  req.finish();

  return boost::make_tuple(request_name, 0);
}

/**
 * process an OPTIONS request.
 */
boost::tuple<string, size_t> process_options_request(request &req) {
  static const string request_name = "OPTIONS";
  const char *origin = req.get_param("HTTP_ORIGIN");
  const char *method = req.get_param("HTTP_ACCESS_CONTROL_REQUEST_METHOD");

  if (origin &&
      (strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {

    // write the response
    req.status(200);
    req.add_header("Content-Type", "text/plain");

    // ensure the request is finished
    req.finish();

  } else {
    process_not_allowed(req);
  }
  return boost::make_tuple(request_name, 0);
}

} // anonymous namespace

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter,
                     const string &generator, routes &route,
                     boost::shared_ptr<data_selection::factory> factory) {
  try {
    // get the client IP address
    string ip = fcgi_get_env(req, "REMOTE_ADDR");
    pt::ptime start_time(pt::second_clock::local_time());

    // check whether the client is being rate limited
    if (!limiter.check(ip)) {
      logger::message(format("Rate limiter rejected request from %1%") % ip);
      throw http::bandwidth_limit_exceeded("You have downloaded too much "
                                           "data. Please try again later.");
    }

    // get the request method
    string method = fcgi_get_env(req, "REQUEST_METHOD");

    // data returned from request methods
    string request_name;
    size_t bytes_written;

    // process request
    if (method == "GET") {
      boost::tie(request_name, bytes_written) =
          process_get_request(req, route, factory, ip, generator);

    } else if (method == "HEAD") {
      boost::tie(request_name, bytes_written) =
          process_head_request(req, route, factory, ip);

    } else if (method == "OPTIONS") {
      boost::tie(request_name, bytes_written) = process_options_request(req);

    } else {
      process_not_allowed(req);
    }

    // update the rate limiter, if anything was written
    if (bytes_written > 0) {
      limiter.update(ip, bytes_written);
    }

    // log the completion time (note: this comes last to avoid
    // logging twice when an error is thrown.)
    pt::ptime end_time(pt::second_clock::local_time());
    logger::message(format("Completed request for %1% from %2% in %3% ms "
                           "returning %4% bytes") %
                    request_name % ip %
                    (end_time - start_time).total_milliseconds() %
                    bytes_written);

  } catch (const http::exception &e) {
    // errors here occur before we've started writing the response
    // so we\ can send something helpful back to the client.
    respond_error(e, req);

  } catch (const std::exception &e) {
    // catch an error here to provide feedback to the user
    respond_error(http::server_error(e.what()), req);

    // re-throw the exception for higher-level handling
    throw;
  }
}
